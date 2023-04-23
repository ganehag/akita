AC_INIT([modbus-akita], [0.1], [mikael.brorsson@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Check for programs
AC_PROG_CC

AC_CHECK_HEADERS([modbus/modbus.h], ,AC_MSG_ERROR([modbus header not found!]))
AC_CHECK_LIB([modbus],
             [modbus_set_bits_from_bytes],
             [AC_SUBST([MODBUS_LIBS], ["-lmodbus"])],
             [AC_MSG_ERROR([modbus library not found!])])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
  Makefile
  src/Makefile
])
AC_OUTPUT
