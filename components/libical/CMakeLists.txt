# libical (https://github.com/libical/libical), vendored unmodified - see UPSTREAM.md. Only the build option
# `agenda-rrule` uses it (main/calendar_rrule.c); without that option this is an empty component, so a build
# without the option has no libical code, no symbol of it and the same size as before it was added.
if(NOT CONFIG_FEATURE_AGENDA_RRULE)
    idf_component_register()
    return()
endif()

set(LIBICAL_SRCS
    libical/byref.c
    libical/icalarray.c
    libical/icalattach.c
    libical/icalcomponent.c
    libical/icaldate_p.c
    libical/icalderivedparameter.c
    libical/icalderivedproperty.c
    libical/icalderivedvalue.c
    libical/icalduration.c
    libical/icalenumarray.c
    libical/icalenums.c
    libical/icalerror.c
    libical/icalerror_p.c
    libical/icallangbind_p.c
    libical/icallimits.c
    libical/icalmemory.c
    libical/icalparameter.c
    libical/icalparser.c
    libical/icalperiod.c
    libical/icalproperty.c
    libical/icalpvl_p.c
    libical/icalrecur.c
    libical/icalrestriction.c
    libical/icalstrarray.c
    libical/icaltime.c
    libical/icaltime_p.c
    libical/icaltimezone.c
    libical/icaltimezone_p.c
    libical/icaltypes.c
    libical/icalvalue.c
    libical/qsort_gen.c
)

idf_component_register(
    SRCS ${LIBICAL_SRCS}
    INCLUDE_DIRS "libical"
    PRIV_INCLUDE_DIRS "port"
)

# libical's sources include <config.h> (ours, in port/) and are built as a static library. Warnings of third-party
# code are not ours to fix: the sources stay as upstream wrote them.
target_compile_definitions(${COMPONENT_LIB} PRIVATE HAVE_CONFIG_H=1)
target_compile_options(${COMPONENT_LIB} PRIVATE -w)
