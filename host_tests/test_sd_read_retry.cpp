// sd_read_retry.c: a failed read of the storage is repeated once from where it started. The stream
// is simulated: a byte string with a read position and a script of failures (the SD driver
// answering "not enough mem" for a moment).

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

extern "C" {
#include "sd_read_retry.h"
}

namespace
{

struct Stream {
    std::string data;
    size_t pos = 0;
    bool err = false;
    // one entry per read attempt: true = fail that attempt after delivering `partial` bytes
    std::vector<bool> fail;
    size_t partial = 0;
    size_t attempts = 0;
    int pauses = 0;
    int failure_logs = 0;
    int result_logs = 0;
    int last_recovered = -1;
    bool tell_fails = false;
    bool seek_fails = false;
};

size_t do_read(void *buf, size_t size, size_t count, void *ctx)
{
    Stream *s = static_cast<Stream *>(ctx);
    size_t want = size * count;
    bool fail = s->attempts < s->fail.size() && s->fail[s->attempts];
    s->attempts++;
    size_t avail = s->data.size() - s->pos;
    size_t n = want < avail ? want : avail;
    if (fail) {
        n = s->partial < n ? s->partial : n;
        s->err = true;
    }
    memcpy(buf, s->data.data() + s->pos, n);
    s->pos += n;
    return n / size;
}
int do_error(void *ctx)
{
    return static_cast<Stream *>(ctx)->err;
}
void do_clear(void *ctx)
{
    static_cast<Stream *>(ctx)->err = false;
}
long do_tell(void *ctx)
{
    Stream *s = static_cast<Stream *>(ctx);
    return s->tell_fails ? -1 : (long) s->pos;
}
int do_seek(void *ctx, long pos)
{
    Stream *s = static_cast<Stream *>(ctx);
    if (s->seek_fails) {
        return -1;
    }
    s->pos = (size_t) pos;
    return 0;
}
void on_failure(void *ctx, size_t, size_t)
{
    static_cast<Stream *>(ctx)->failure_logs++;
}
void do_pause(void *ctx)
{
    static_cast<Stream *>(ctx)->pauses++;
}
void on_result(void *ctx, int recovered)
{
    Stream *s = static_cast<Stream *>(ctx);
    s->result_logs++;
    s->last_recovered = recovered;
}

const sd_read_ops_t kOps = {do_read, do_error,   do_clear, do_tell,
                            do_seek, on_failure, do_pause, on_result};

TEST(SdReadRetry, ASuccessfulReadIsPassedThroughUntouched)
{
    Stream s;
    s.data = "0123456789";
    char buf[8] = {};
    int retried = -1;
    EXPECT_EQ(sd_read_with_retry(&kOps, &s, buf, 1, 4, &retried), 4u);
    EXPECT_EQ(std::string(buf, 4), "0123");
    EXPECT_EQ(retried, 0);
    EXPECT_EQ(s.attempts, 1u);
    EXPECT_EQ(s.pauses, 0);
    EXPECT_EQ(s.failure_logs, 0);
}

TEST(SdReadRetry, TheEndOfTheFileIsNotAFailure)
{
    Stream s;
    s.data = "abc";
    char buf[8] = {};
    int retried = -1;
    EXPECT_EQ(sd_read_with_retry(&kOps, &s, buf, 1, 8, &retried), 3u);  // short, but no error
    EXPECT_EQ(retried, 0);
    EXPECT_EQ(s.attempts, 1u);
    EXPECT_EQ(s.failure_logs, 0);
}

TEST(SdReadRetry, AFailedReadIsRepeatedOnceFromWhereItStarted)
{
    Stream s;
    s.data = "0123456789";
    s.pos = 2;
    s.fail = {true, false};
    s.partial = 3;  // the failing attempt delivered 3 bytes and moved the position
    char buf[8] = {};
    int retried = -1;
    EXPECT_EQ(sd_read_with_retry(&kOps, &s, buf, 1, 5, &retried), 5u);
    EXPECT_EQ(std::string(buf, 5),
              "23456");  // the whole block, not the rest after the partial read
    EXPECT_EQ(retried, 1);
    EXPECT_EQ(s.attempts, 2u);
    EXPECT_EQ(s.pos, 7u);
    EXPECT_EQ(s.pauses, 1);
    EXPECT_EQ(s.failure_logs, 1);
    EXPECT_EQ(s.result_logs, 1);
    EXPECT_EQ(s.last_recovered, 1);
    EXPECT_FALSE(s.err);
}

TEST(SdReadRetry, ASecondFailureIsWhatTheCallerGets)
{
    Stream s;
    s.data = "0123456789";
    s.fail = {true, true};
    s.partial = 0;
    char buf[8] = {};
    int retried = -1;
    EXPECT_EQ(sd_read_with_retry(&kOps, &s, buf, 1, 5, &retried), 0u);
    EXPECT_EQ(retried, 1);
    EXPECT_EQ(s.attempts, 2u);  // once more, not a loop
    EXPECT_EQ(s.last_recovered, 0);
    EXPECT_TRUE(s.err);  // the stream is left in the error state fread() would leave it in
}

TEST(SdReadRetry, WithoutAPositionThereIsNoWayBackAndNoRetry)
{
    Stream s;
    s.data = "0123456789";
    s.fail = {true, false};
    s.tell_fails = true;
    char buf[8] = {};
    int retried = -1;
    EXPECT_EQ(sd_read_with_retry(&kOps, &s, buf, 1, 5, &retried), 0u);
    EXPECT_EQ(retried, 0);
    EXPECT_EQ(s.attempts, 1u);
    EXPECT_EQ(s.failure_logs, 1);  // it is still reported
}

TEST(SdReadRetry, ASeekThatFailsEndsTheAttempt)
{
    Stream s;
    s.data = "0123456789";
    s.fail = {true, false};
    s.seek_fails = true;
    char buf[8] = {};
    int retried = -1;
    sd_read_with_retry(&kOps, &s, buf, 1, 5, &retried);
    EXPECT_EQ(retried, 0);
    EXPECT_EQ(s.attempts, 1u);
}

TEST(SdReadRetry, ItemsLargerThanOneByteAreCountedInItems)
{
    Stream s;
    s.data = std::string(64, 'x');
    s.fail = {true, false};
    s.partial = 10;
    char buf[64] = {};
    int retried = -1;
    EXPECT_EQ(sd_read_with_retry(&kOps, &s, buf, 4, 8, &retried), 8u);  // 8 items of 4 bytes
    EXPECT_EQ(retried, 1);
}

TEST(SdReadRetry, NullHooksAreFine)
{
    Stream s;
    s.data = "0123456789";
    s.fail = {true, false};
    sd_read_ops_t ops = kOps;
    ops.on_failure = nullptr;
    ops.pause = nullptr;
    ops.on_retry_result = nullptr;
    char buf[8] = {};
    EXPECT_EQ(sd_read_with_retry(&ops, &s, buf, 1, 4, nullptr), 4u);
}

}  // namespace
