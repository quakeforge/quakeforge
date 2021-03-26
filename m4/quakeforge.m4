dnl check for fields in a structure
dnl
dnl AC_HAVE_STRUCT_FIELD(struct, field, headers)

AC_DEFUN([AC_HAVE_STRUCT_FIELD], [
define(cache_val, translit(ac_cv_type_$1_$2, [A-Z ], [a-z_]))
AC_CACHE_CHECK([for $2 in $1], cache_val,[
AC_TRY_COMPILE([$3],[$1 x; x.$2;],
cache_val=yes,
cache_val=no)])
if test "$cache_val" = yes; then
	define(foo, translit(HAVE_$1_$2, [a-z ], [A-Z_]))
	AC_DEFINE(foo, 1, [Define if $1 has field $2.])
	undefine(foo)
fi
undefine(cache_val)
])

dnl Checks if function/macro va_copy() is available
dnl Defines HAVE_VA_COPY on success.
AC_DEFUN([AC_FUNC_VA_COPY],
[AC_CACHE_CHECK([for va_copy], ac_cv_func_va_copy,
				[AC_TRY_LINK([
#ifdef  HAVE_STDARG_H
#   include <stdarg.h>
#else
#   include <varargs.h>
#endif],
		[
va_list a, b;

va_copy(a, b);],
				[ac_cv_func_va_copy=yes],
				[ac_cv_func_va_copy=no])])
if test $ac_cv_func_va_copy = yes; then
  AC_DEFINE(HAVE_VA_COPY, 1, [Define if va_copy is available])
fi])

dnl Checks if function/macro __va_copy() is available
dnl Defines HAVE__VA_COPY on success.
AC_DEFUN([AC_FUNC__VA_COPY],
[AC_CACHE_CHECK([for __va_copy], ac_cv_func__va_copy,
				[AC_TRY_LINK([
#ifdef  HAVE_STDARG_H
#   include <stdarg.h>
#else
#   include <varargs.h>
#endif],
		[
va_list a, b;

__va_copy(a, b);],
				[ac_cv_func__va_copy=yes],
				[ac_cv_func__va_copy=no])])
if test $ac_cv_func__va_copy = yes; then
  AC_DEFINE(HAVE__VA_COPY, 1, [Define if __va_copy is available])
fi])

dnl Checks if va_list is an array
dnl Defines VA_LIST_IS_ARRAY on success.
AC_DEFUN([AC_TYPE_VA_LIST],
[AC_CACHE_CHECK([if va_list is an array], ac_cv_type_va_list_array,
				[AC_TRY_LINK([
#ifdef  HAVE_STDARG_H
#   include <stdarg.h>
#else
#   include <varargs.h>
#endif
],
	[
va_list a, b;

a = b;],
		[ac_cv_type_va_list_array=no],
		[ac_cv_type_va_list_array=yes])]) 
if test $ac_cv_type_va_list_array = yes; then
	AC_DEFINE(VA_LIST_IS_ARRAY, 1, [Define if va_list is an array])
fi])

AC_DEFUN([QF_SUBST],
[m4_ifset([AM_SUBST_NOTMAKE],[AM_SUBST_NOTMAKE([$1])], []) AC_SUBST([$1])]);

AC_DEFUN([QF_DEPS], [
$1_INCS='m4_normalize($2)'
$1_DEPS='m4_normalize($3)'
$1_LIBS='m4_normalize($3) m4_normalize($4)'
QF_SUBST($1_INCS)
QF_SUBST($1_DEPS)
QF_SUBST($1_LIBS)])

AC_DEFUN([QF_NEED], [m4_map_args_w([$2], [$1_need_], [=yes], [;])])

AC_DEFUN([QF_PROCESS_NEED_subroutine],
[m4_foreach_w([qfn_need], [$5],
[if test x"${$2[_need_]qfn_need}" = xyes; then
	$4="${$4} m4_join([/],[$6],[$1])[]qfn_need[]$3"
fi
])])

AC_DEFUN([QF_PROCESS_NEED_FUNC],
[m4_foreach_w([qfn_need], [$2],
[if test x"${$1[_need_]qfn_need}" = xyes; then
	$3
fi
])])

AC_DEFUN([QF_PROCESS_NEED_LIBS],
[m4_define([qfn_ext], m4_default($4,[la]))
QF_PROCESS_NEED_subroutine([lib$1_],[$1],[.]qfn_ext,[$1_libs],[$2],[$3])
QF_SUBST([$1_libs])])

AC_DEFUN([QF_PROCESS_NEED_LIST],
[QF_PROCESS_NEED_subroutine([],[$1],[],[$1_list],[$2])
QF_SUBST([$1_list])])

AC_DEFUN([QF_DEFAULT_PLUGIN],
[m4_define([qfn_default], m4_default($3,$1)[_default])
if test -z "${qfn_default}"; then
	QF_PROCESS_NEED_FUNC([$1],[$2],[qfn_default=qfn_need])
fi
AC_DEFINE_UNQUOTED(m4_toupper(qfn_default), ["${qfn_default}"], [Define to default the $1 plugin])
])

AC_DEFUN([QF_PROCESS_NEED_PLUGINS],
[QF_PROCESS_NEED_subroutine([$1_],[$1],[.la],[$1_plugins],[$2],[$3])
QF_SUBST([$1_plugins])
QF_DEFAULT_PLUGIN([$1],[$2],[$4])
AC_DEFINE_UNQUOTED(m4_toupper(m4_default($4,$1)[_plugin_protos]), [], [list of $1 plugin prototypes])
AC_DEFINE_UNQUOTED(m4_toupper(m4_default($4,$1)[_plugin_list]), [{0, 0}], [list of $1 plugins])
])

AC_DEFUN([QF_STATIC_PLUGIN_LIBS],
[QF_PROCESS_NEED_subroutine(m4_join([/],[$4],[$2_]),[$2],[.la],[$1_static_plugin_libs],[$3])
QF_SUBST([$1_static_plugin_libs])])

AC_DEFUN([QF_STATIC_PLUGIN_PROTOS],
[QF_PROCESS_NEED_subroutine([extern plugin_t *$2_],[$2],[_PluginInfo (void);],[$1_plugin_protos],[$3])
AC_DEFINE_UNQUOTED(m4_toupper([$1_plugin_protos]), [${$1_plugin_protos}], [list of $1 plugin prototypes])])

AC_DEFUN([QF_STATIC_PLUGIN_LIST],
[$1_plugin_list="{0, 0}"
m4_foreach_w([qfn_need], [$3],
[if test x"${$2[_need_]qfn_need}" = xyes; then
	$1_plugin_list="{\"$2_[]qfn_need\", $2_[]qfn_need[]_PluginInfo},${$1_plugin_list}"
fi
])
AC_DEFINE_UNQUOTED(m4_toupper([$1_plugin_list]), [${$1_plugin_list}], [list of $1 plugins])])

AC_DEFUN([QF_PROCESS_NEED_STATIC_PLUGINS],
[QF_PROCESS_NEED_subroutine([$1_],[$1],[.la],m4_default($4,$1)[_static_plugins],[$2],[$3])
QF_SUBST(m4_default($4,$1)[_static_plugins])
QF_DEFAULT_PLUGIN([$1],[$2],[$4])
QF_STATIC_PLUGIN_LIBS(m4_default($4,$1),[$1],[$2],[$3])
QF_STATIC_PLUGIN_PROTOS(m4_default($4,$1),[$1],[$2])
QF_STATIC_PLUGIN_LIST(m4_default($4,$1),[$1],[$2])])

AC_DEFUN([QF_WITH_TARGETS], [
AC_ARG_WITH($1,
	[$2]
	[$3], $1="$withval", $1=all
)
if test "x${$1}" = "xall"; then
	for qf_t in `echo '$3' | sed -e "s/,/ /g"`''; do
		eval ENABLE_$1_${qf_t}=yes
	done
else
	for qf_t in `echo '$3' | sed -e "s/,/ /g"`''; do
		if echo ",${$1}," | grep ",$qf_t," > /dev/null; then
			eval ENABLE_$1_${qf_t}=yes
		else
			eval ENABLE_$1_${qf_t}=no
		fi
	done
fi
])

AC_DEFUN([QF_CC_OPTION_TEST], [
AC_MSG_CHECKING(whether $1 works)
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $1"
qf_opt_ok=no
AC_TRY_COMPILE(
	[],
	[],
	qf_opt_ok=yes
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
)
CFLAGS="$save_CFLAGS"
if test "x$qf_opt_ok" = xyes; then
	true
	$2
else
	true
	$3
fi
])

AC_DEFUN([QF_CC_OPTION], [
QF_CC_OPTION_TEST([$1], [CFLAGS="$CFLAGS $1"])
])
