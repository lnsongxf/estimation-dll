dnl ax_mexext.m4 --- check for MEX-file suffix.
dnl
dnl Copyright (C) 2000--2003 Ralph Schleicher
dnl Copyright (C) 2009 Dynare Team
dnl
dnl This program is free software; you can redistribute it and/or
dnl modify it under the terms of the GNU General Public License as
dnl published by the Free Software Foundation; either version 2,
dnl or (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License along
dnl with this program. If not, see <http://www.gnu.org/licenses/>.
dnl
dnl As a special exception to the GNU General Public License, if
dnl you distribute this file as part of a program that contains a
dnl configuration script generated by GNU Autoconf, you may include
dnl it under the same distribution terms that you use for the rest
dnl of that program.
dnl
dnl Code:

# AX_MEXEXT
# ---------
# Check for MEX-file suffix.
AC_DEFUN([AX_MEXEXT],
[dnl
AC_PREREQ([2.50])
AC_REQUIRE([AX_MATLAB])
AC_REQUIRE([AX_MATLAB_VERSION])
AC_REQUIRE([AC_CANONICAL_BUILD])
AC_CACHE_CHECK([for MEX-file suffix], [ax_cv_mexext],
[if test "${MEXEXT+set}" = set ; then
    ax_cv_mexext="$MEXEXT"
else
    # The mexext script appeared in MATLAB 7.1
    AX_COMPARE_VERSION([$MATLAB_VERSION], [lt], [7.1], [AC_MSG_ERROR([I can't determine the MEX file extension. Please explicitly indicate it to the configure script with the MEXEXT variable.])])
    case $build_os in
      *cygwin*)
        ax_cv_mexext=`$MATLAB/bin/mexext.bat | sed 's/\r//'`
        ;;
      *mingw*)
        ax_cv_mexext=`cd $MATLAB/bin && cmd /c mexext.bat | sed 's/\r//'`
        ;;
      *)
        ax_cv_mexext=`$MATLAB/bin/mexext`
        ;;
    esac
fi])
MEXEXT="$ax_cv_mexext"
AC_SUBST([MEXEXT])
])

# AX_DOT_MEXEXT
# -------------
# Check for MEX-file suffix with leading dot.
AC_DEFUN([AX_DOT_MEXEXT],
[dnl
AC_REQUIRE([AX_MEXEXT])
case $MEXEXT in
  .*)
    ;;
  *)
    if test -n "$MEXEXT" ; then
	MEXEXT=.$MEXEXT
	AC_MSG_RESULT([setting MEX-file suffix to $MEXEXT])
	AC_SUBST([MEXEXT])
    fi
    ;;
esac
])

dnl ax_mexext.m4 ends here

dnl Local variables:
dnl tab-width: 8
dnl End:
