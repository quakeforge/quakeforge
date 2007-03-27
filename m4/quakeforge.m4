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


AC_DEFUN([QF_DEPS], [
$1_INCS='$2'
$1_DEPS='$3'
$1_LIBS='$3 $4 '
AC_SUBST($1_INCS)
AC_SUBST($1_DEPS)
AC_SUBST($1_LIBS)
])

AC_DEFUN([QF_NEED], [
for qfn_lib in $2; do
	eval "$1_need_$qfn_lib=yes"
done
])

AC_DEFUN([QF_PROCESS_NEED], [
if test -n "$3"; then
	qfn_ext="$3"
else
	qfn_ext=la
fi
for qfn_lib in $2; do
	if eval test x'"${$1_need_'$qfn_lib'}"' = xyes; then
		qfn_tmp="${$1_libs} lib$1_$qfn_lib.$qfn_ext"
		eval '$1_libs="$qfn_tmp"'
	fi
done
AC_SUBST($1_libs)
])

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
