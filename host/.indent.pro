/* Derived from GNU Indent for the "-linux" option */
/* Divergence marked by keeping old setting in preceding comment */

--no-blank-lines-after-declarations
--blank-lines-after-procedures
--no-blank-lines-after-commas
--break-before-boolean-operator
--honour-newlines
--braces-on-if-line
--braces-on-struct-decl-line
// -- Can't be inhibited ?
--comment-indentation33
// -- Can't be inhibited ?
--declaration-comment-column33
--no-comment-delimiters-on-blank-lines
--cuddle-else
--continuation-indentation4

--case-indentation0
--line-comments-indentation0
--declaration-indentation1
--dont-format-first-column-comments
/* --indent-level8 */
--indent-level4
--parameter-indentation0
--line-length80
--continue-at-parentheses
--no-space-after-function-call-names
--no-space-after-parentheses
--dont-break-procedure-type
--space-after-if

--space-after-for
--space-after-while
--no-space-after-casts
--dont-star-comments
--swallow-optional-blank-lines
--dont-format-comments
// -- Can't be inhibited ?
--else-endif-column33
--space-special-semicolon
/* --tab-size8 */
--tab-size4
/* --indent-label1 */
--indent-label0

/* Additional Indent options */
--leave-preprocessor-space
--no-tabs
--preserve-mtime
--case-indentation4

-T bladerf
-T libusb_version
-T version_fn
-T numeric_suffix
-T clockid_t
-T SHA256_CTX
-T imaxdiv_t
-T bool
-T clockid_t
-T bladerf_device_t
-T bladerf_backend
-T bladerf_dev_speed
-T bladerf_loopback
-T bladerf_lpf_mode
-T bladerf_module
-T bladerf_xb
-T bladerf_xb200_filter
-T bladerf_xb200_path
-T bladerf_cal_module
-T bladerf_correction
-T bladerf_format
-T bladerf_lna_gain
-T bladerf_sampling
-T bladerf_stream_cb
-T bladerf_fpga_size
-T bladerf_log_level
-T bladerf_image_type
-T bladerf_backend
-T bladerf_stream_state
-T backend_probe_target
-T transfer_status
-T corr_type
-T usb_target
-T usb_request
-T usb_direction
-T word
-T byte
-T lms_bw
-T lms_lna
-T lms_lbp
-T lms_pa
-T state
-T BY_HANDLE_FILE_INFORMATION
-T stat
-T sync_buffer_status
-T sync_meta_state
-T sync_state
-T sync_worker_state
-T task_state

/* Some windows types and windows derived typedefs */
-T HRESULT
-T CALLBACK
-T LONG
-T LPFNSHGKFP_T
-T LPFNREGOPEN_T
-T LPFNREGQUERY_T
-T LPFNREGCLOSE_T
-T HKEY
-T LPCTSTR 
-T DWORD 
-T REGSAM 
-T PHKEY
-T HKEY
-T LPCTSTR
-T LPDWORD
-T LPDWORD
-T LPBYTE
-T LPDWORD
-T HINSTANCE 
-T FILETIME

/* C99 stdint.h types GNU indent seems unaware of */
-T int8_t
-T int16_t
-T int32_t
-T int64_t
-T uint8_t
-T uint16_t
-T uint32_t
-T uint64_t
