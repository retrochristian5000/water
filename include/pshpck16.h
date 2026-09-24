/*
 * 16-byte structure packing support.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#if !(defined(lint) || defined(RC_INVOKED))
# if (defined(_MSC_VER) && (_MSC_VER >= 800) && !defined(_M_I86)) || \
     defined(_PUSHPOP_SUPPORTED) || defined(__GNUC__) || defined(__clang__)
#  ifdef _MSC_VER
#   pragma warning(disable:4103)
#  endif
#  if defined(MIDL_PASS) && !defined(__midl)
#   pragma pack(16)
#  else
#   pragma pack(push,16)
#  endif
# else
#  pragma pack(16)
# endif
#endif
