#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_DEFUN([CHECK_ENABLE_DEBUG],[
    AC_BEFORE([$0],[AC_PROG_CC])dnl
    AC_BEFORE([$0],[AC_PROG_CXX])dnl

    AC_MSG_CHECKING(whether to enable debugging)

    ax_enable_debug_default=m4_tolower(m4_normalize(ifelse([$1],,[no],[$1])))
    ax_enable_debug_is_release=m4_tolower(m4_normalize(ifelse([$4],,
                                                              [$ax_is_release],
                                                              [$4])))

    # If this is a release, override the default.
    AS_IF([test "$ax_enable_debug_is_release" = "yes"],
      [ax_enable_debug_default="no"])

    m4_define(ax_enable_debug_vars,[m4_normalize(ifelse([$2],,,[$2]))])
    m4_define(ax_disable_debug_vars,[m4_normalize(ifelse([$3],,[NDEBUG],[$3]))])

    AC_ARG_ENABLE(debug,
	[AS_HELP_STRING([--enable-debug=]@<:@yes/info/profile/no@:>@,[compile with debugging])],
	[],enable_debug=$ax_enable_debug_default)

    # empty means no debugging
    AS_IF([test "x$enable_debug" = "x"],
      [enable_debug="no"])

    # case of debug
    AS_CASE([$enable_debug],
      [yes],[
	AC_MSG_RESULT(yes)
	CFLAGS="${CFLAGS} -g -O0"
	CXXFLAGS="${CXXFLAGS} -g -O0"
      ],
      [info],[
	AC_MSG_RESULT(info)
	CFLAGS="${CFLAGS} -g"
	CXXFLAGS="${CXXFLAGS} -g"
      ],
      [profile],[
    AC_MSG_RESULT(profile)
	CFLAGS="${CFLAGS} -g -pg"
	CXXFLAGS="${CXXFLAGS} -g -pg"
	LDFLAGS="${LDFLAGS} -pg"
      ],
      [
	AC_MSG_RESULT(no)
	CFLAGS="${CFLAGS} -O3"
	CXXFLAGS="${CXXFLAGS} -O3"
	LDFLAGS="${LDFLAGS} -O3"
      ])

    dnl Define various variables if debugging is disabled.
    dnl assert.h is a NOP if NDEBUG is defined, so define it by default.
    AS_IF([test "x$enable_debug" = "xyes"],
      [m4_map_args_w(ax_enable_debug_vars, [AC_DEFINE(], [,,[Define if debugging is enabled])])],
      [m4_map_args_w(ax_disable_debug_vars, [AC_DEFINE(], [,,[Define if debugging is disabled])])])
    ax_enable_debug=$enable_debug
])

AC_DEFUN([CHECK_COMPILER_WRAPPER],
[
AC_ARG_WITH(compiler_wrapper,
[  --with-compiler-wrapper[=path/to/wrapper]
    Enable compiling with wrappers such as distcc and ccache],
    COMPILER_WRAPPER=$withval, COMPILER_WRAPPER="no")

AC_ARG_WITH(ccache,
[  --with-ccache[=path/to/ccache]
                          Enable compiling with ccache],
    CCACHE=$withval, CCACHE="no")

if test "$CCACHE" != "no"; then
    if test -z "$CCACHE" -o "$CCACHE" = "yes"; then
        CCACHE=
    else
        if test ! -e "$CCACHE"; then
            AC_MSG_ERROR([$CCACHE not found])
        fi
    fi
    AC_PATH_PROGS(CCACHE, $CCACHE ccache)
    if test -z "$CCACHE" -o "$CCACHE" = ":"; then
        AC_MSG_ERROR([ccache not found])
    elif test -x "$CCACHE"; then
        if test "$COMPILER_WRAPPER" != "no"; then
            COMPILER_WRAPPER="$CCACHE $COMPILER_WRAPPER"
        else
            COMPILER_WRAPPER="$CCACHE"
        fi
    else
        AC_MSG_ERROR([$CCACHE is not executable])
    fi
fi

if test "$COMPILER_WRAPPER" != "no"; then
    case "$CC" in
    $COMPILER_WRAPPER\ *)
        :
        ;;
    *)
        CC="$COMPILER_WRAPPER $CC"
        CXX="$COMPILER_WRAPPER $CXX"
        ;;
    esac
fi

])

AC_PREREQ([2.69])
AC_INIT([dynamic], [1.0])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([config.h])

CHECK_ENABLE_DEBUG(no, [DEBUG], [NDEBUG])

# Checks for programs.
AC_PROG_CC([gcc clang])
AC_PROG_CXX([g++ clang++])
#AC_PROG_INSTALL

CHECK_COMPILER_WRAPPER

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset pow])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT

