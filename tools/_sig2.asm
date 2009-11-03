[global _default_sig_handler]
[extern __default_sig_handler]

_default_sig_handler:
  call [__default_sig_handler]
  add  esp, 8
  popa
  popf
  ret
