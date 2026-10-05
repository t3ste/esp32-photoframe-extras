#include <stdlib.h>
#include <string.h>

#include "sdkconfig.h"

// Part of the `agenda` build option, which switches
// CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_CROSS_SIGNED_VERIFY on (see
// features/sdkconfig.defaults.agenda). Without that option there is nothing to fix.
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_CROSS_SIGNED_VERIFY)

#include "esp_memory_utils.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

/**
 * @file tls_ca_cb_fix.c
 * @brief Stops every TLS connection from leaking about 200-400 bytes of internal heap.
 *
 * With the cross-signed verification of the certificate bundle, ESP-IDF's esp_crt_bundle.c
 * registers a callback (mbedtls_ssl_conf_ca_cb) that builds a certificate for the trusted
 * root out of the bundle: a mbedtls_x509_crt whose subject_raw buffer and the oid/val buffers
 * of every name entry are separate calloc()s. mbedtls hands that certificate to
 * mbedtls_x509_crt_free() after the verification, and that frees only the structure, the list
 * nodes and the buffer that holds a parsed certificate (raw, if own_buffer is set) - not those
 * separate buffers, which are lost with every connection.
 *
 * The fix puts the separate buffers into one block that the certificate owns as its raw buffer,
 * so mbedtls_x509_crt_free() frees all of it. It is installed by wrapping
 * mbedtls_ssl_conf_ca_cb (the link option --wrap=mbedtls_ssl_conf_ca_cb, main/CMakeLists.txt):
 * the callback of the bundle is replaced by one that calls it and then packs what it returned.
 * A certificate that has a raw buffer (one that was parsed) is left alone, and so is one that
 * does not own what it points to. ESP-IDF 6.0.3 and later (the CI image follows the
 * release-v6.0 branch) fixed the leak itself: the callback now points subject_raw into the
 * bundle in flash and the name entries into the *child's* issuer buffers, so there is nothing
 * of the certificate's own to free - freeing it anyway crashes (free() of a flash address, seen
 * as an assert in the OTA check of v219.0.0). The two tests in owns_its_buffers() tell the two
 * behaviours apart; if the block cannot be allocated the certificate stays as it was.
 */

static mbedtls_x509_crt_ca_cb_t s_bundle_cb;  // the callback of esp_crt_bundle.c

// Moves one separately allocated buffer into the block.
static void adopt(unsigned char **field, size_t len, unsigned char *block, size_t *used)
{
    unsigned char *old = *field;
    if (old == NULL) {
        return;
    }
    if (len > 0) {
        memcpy(block + *used, old, len);
        *field = block + *used;
        *used += len;
    } else {
        *field = NULL;
    }
    free(old);
}

// True for a certificate whose subject_raw and name buffers are separate heap allocations of the
// callback (older ESP-IDF); false when it only references data owned by somebody else (flash
// bundle, the child certificate) - those must not be freed or moved.
static bool owns_its_buffers(const mbedtls_x509_crt *cert, const mbedtls_x509_crt *child)
{
    const unsigned char *raw = cert->subject_raw.p;
    if (raw == NULL || !(esp_ptr_in_dram(raw) || esp_ptr_external_ram(raw))) {
        return false;  // flash (the bundle) or nothing: not an allocation
    }
    if (child != NULL && cert->subject.oid.p != NULL &&
        cert->subject.oid.p == child->issuer.oid.p) {
        return false;  // the name entries are the child's own buffers
    }
    return true;
}

// What the buffers of a certificate made by the bundle's callback add up to.
static size_t separate_bytes(const mbedtls_x509_crt *cert)
{
    size_t total = cert->subject_raw.p ? cert->subject_raw.len : 0;
    for (const mbedtls_asn1_named_data *name = &cert->subject; name != NULL; name = name->next) {
        total += name->oid.p ? name->oid.len : 0;
        total += name->val.p ? name->val.len : 0;
    }
    return total;
}

static void pack_candidates(mbedtls_x509_crt *list, const mbedtls_x509_crt *child)
{
    for (mbedtls_x509_crt *cert = list; cert != NULL; cert = cert->next) {
        if (cert->raw.p != NULL) {
            continue;  // parsed from a buffer: nothing separate to free
        }
        if (!owns_its_buffers(cert, child)) {
            continue;  // newer ESP-IDF: references only, nothing leaks
        }
        size_t total = separate_bytes(cert);
        unsigned char *block = malloc(total > 0 ? total : 1);
        if (block == NULL) {
            continue;  // as before: it leaks, but it works
        }
        size_t used = 0;
        adopt(&cert->subject_raw.p, cert->subject_raw.len, block, &used);
        for (mbedtls_asn1_named_data *name = &cert->subject; name != NULL; name = name->next) {
            adopt(&name->oid.p, name->oid.len, block, &used);
            adopt(&name->val.p, name->val.len, block, &used);
        }
        cert->raw.p = block;
        cert->raw.len = used;
        cert->MBEDTLS_PRIVATE(own_buffer) = 1;  // mbedtls_x509_crt_free() frees (and wipes) it
    }
}

static int packing_ca_cb(void *ctx, mbedtls_x509_crt const *child, mbedtls_x509_crt **candidate_cas)
{
    int ret = s_bundle_cb(ctx, child, candidate_cas);
    if (ret == 0 && candidate_cas != NULL && *candidate_cas != NULL) {
        pack_candidates(*candidate_cas, child);
    }
    return ret;
}

extern void __real_mbedtls_ssl_conf_ca_cb(mbedtls_ssl_config *conf,
                                          mbedtls_x509_crt_ca_cb_t f_ca_cb, void *p_ca_cb);

// The only user of mbedtls_ssl_conf_ca_cb in the firmware is the certificate bundle; a callback
// that is not that one is passed on as it is.
void __wrap_mbedtls_ssl_conf_ca_cb(mbedtls_ssl_config *conf, mbedtls_x509_crt_ca_cb_t f_ca_cb,
                                   void *p_ca_cb)
{
    if (f_ca_cb != NULL && (s_bundle_cb == NULL || s_bundle_cb == f_ca_cb)) {
        s_bundle_cb = f_ca_cb;
        f_ca_cb = packing_ca_cb;
    }
    __real_mbedtls_ssl_conf_ca_cb(conf, f_ca_cb, p_ca_cb);
}

#endif  // CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_CROSS_SIGNED_VERIFY
