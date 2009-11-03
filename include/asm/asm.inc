%ifndef ASM_H
  %define ASM_H

  %ifdef ASM_USCORE

    %macro EXPORT 1
      [global  %1]
	  [global  _%1]
	  _%1:
      %1:
    %endmacro


    %macro IMPORT 1
      [extern  _%1]
	  %define %1 _%1
    %endmacro

  %else

    %macro EXPORT 1
	  [global %1]
	  %1:
    %endmacro

    %macro IMPORT 1
	  [extern %1]
    %endmacro

  %endif
%endif

