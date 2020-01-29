# This file selects the correct makefile for Microsoft NMAKE and GNU make

# Theory of operation:
#
# Microsoft NMAKE considers the caret (^) as an escape character. It is
# possible to specify a literal newline character in a macro by using a
# caret at the end of a line. The next line will then be consumed as part
# of the macro definition. GNU make does not consider a caret as anything
# special, so macro definitions that end in a caret will consume just a
# single line.
#
# This distinction can be used to hide single lines from NMAKE. Putting
# GNU make-specific directives on those lines can then be used to hide
# NMAKE-specific portions from GNU make.

GNU_MAKE_IF=^
ifeq (0, 1)                             # This line only interpreted by GNU make

# Include NMAKE makefile                #                                   #
!INCLUDE nmake.mk                       #                                   #
#                                       # < This portion only seen by NMAKE #
# Begin skip everything else for NMAKE  #                                   #
!IF 0                                   #                                   #

GNU_MAKE_ELSE=^
else                                    # This line only interpreted by GNU make

# Include GNU make makefile             #                                      #
include gnu_make.mk                     # < This portion only seen by GNU make #
#                                       #                                      #

GNU_MAKE_ENDIF=^
endif                                   # This line only interpreted by GNU make

GNU_MAKE_IF_2=^
ifeq (0, 1)                             # This line only interpreted by GNU make

# End skip everything else for NMAKE    #                                   #
!ENDIF                                  # < This portion only seen by NMAKE #
#                                       #                                   #

GNU_MAKE_ENDIF_2=^
endif                                   # This line only interpreted by GNU make
