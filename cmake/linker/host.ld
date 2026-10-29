SECTIONS
{
  .ek_export : ALIGN(8)
  {
    _ek_export_fn_start = .;
    KEEP(*(.ek_export_fn*))
    _ek_export_fn_end = .;
  }
  .ek_static_alloc : ALIGN(8)
  {
    _ek_static_alloc_start = .;
    KEEP(*(SORT(.ek_static_alloc*)))
    _ek_static_alloc_end = .;
  }
}
INSERT AFTER .data;
