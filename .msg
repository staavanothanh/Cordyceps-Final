refactor: remove hardcoded conn safe from tune path  
  
- Fix SECOND 7-value parser bug (duplicate assignment overwrote data)  
- Add g_tune_w7/w8 for connectivity+safe dynamic weights  
- set_tune_weights now 9-param with defaults (backward compat)  
- deploy_side_weights passes all 9 fields  
  
Co-authored-by: CommandCodeBot <noreply@commandcode.ai>  
