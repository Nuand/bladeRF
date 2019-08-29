%
% Copyright (c) 2015-2018 Nuand LLC
%
% Permission is hereby granted, free of charge, to any person obtaining a copy
% of this software and associated documentation files (the "Software"), to deal
% in the Software without restriction, including without limitation the rights
% to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
% copies of the Software, and to permit persons to whom the Software is
% furnished to do so, subject to the following conditions:
%
% The above copyright notice and this permission notice shall be included in
% all copies or substantial portions of the Software.
%
% THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
% IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
% FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
% AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
% LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
% OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
% THE SOFTWARE.
%

% Background info:
% https://www.mathworks.com/help/matlab/matlab_external/passing-arguments-to-shared-library-functions.html#f44412
% https://ofekshilon.com/2016/07/15/on-matlabs-loadlibrary-proto-file-and-pcwin64-thunk/

function [methodinfo,structs,enuminfo,ThunkLibName]=libbladeRF_proto
%LIBBLADERF_PROTO Create structures to define interfaces found in libbladeRF.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Setup
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

ival={cell(1, 0)};
structs=[];
enuminfo=[];
fcnNum=1;
fcns=struct('name',         ival, ...
            'calltype',     ival, ...
            'LHS',          ival, ...
            'RHS',          ival, ...
            'alias',        ival, ...
            'thunkname',    ival);

arch = computer('arch');
switch arch
    case 'glnxa64'
        libname   = 'libbladeRF_thunk_glnxa64';
        libsuffix = '.so';
        u64_type  = 'ulong';

    case 'win64'
        libname   = 'libbladeRF_thunk_pcwin64';
        libsuffix = '.dll';
        u64_type  = 'uint64';

    case 'imac64'
        % Additional type changes for this may be required for OSX support
        libname   = 'libbladeRF_thunk_imac64';
        libsuffix = '.dylib';
        u64_type  = 'ulong';

    otherwise
        error(['Unsupported architecture: ' arch]);
end

search_path = [ '.'; strsplit(path, pathsep)'];
for n = 1:length(search_path)
    to_check = fullfile(search_path{n}, [libname libsuffix]);
    if exist(to_check, 'file')
        %fprintf('Found %s\n', to_check);
        ThunkLibName = to_check;
        break
    else
        %fprintf('%s does not exist.\n', to_check);
    end
end

if ~exist('ThunkLibName', 'var')
    error(['Failed to find ' [libname libsuffix] '. Check path().']);
end

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Functions
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% int bladerf_get_device_list ( struct bladerf_devinfo ** devices );
fcns.thunkname{fcnNum}='int32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_device_list'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerf_devinfoPtrPtr'};fcnNum=fcnNum+1;

% void bladerf_free_device_list ( struct bladerf_devinfo * devices );
fcns.thunkname{fcnNum}='voidvoidPtrThunk';fcns.name{fcnNum}='bladerf_free_device_list'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={'bladerf_devinfoPtr'};fcnNum=fcnNum+1;

% void bladerf_init_devinfo ( struct bladerf_devinfo * info );
fcns.thunkname{fcnNum}='voidvoidPtrThunk';fcns.name{fcnNum}='bladerf_init_devinfo'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={'bladerf_devinfoPtr'};fcnNum=fcnNum+1;

% int bladerf_get_devinfo ( struct bladerf * dev , struct bladerf_devinfo * info );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_devinfo'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_devinfoPtr'};fcnNum=fcnNum+1;

% int bladerf_get_devinfo_from_str ( const char * devstr , struct bladerf_devinfo * info );
fcns.thunkname{fcnNum}='int32cstringvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_devinfo_from_str'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'cstring', 'bladerf_devinfoPtr'};fcnNum=fcnNum+1;

% _Bool bladerf_devinfo_matches ( const struct bladerf_devinfo * a , const struct bladerf_devinfo * b );
fcns.thunkname{fcnNum}='_BoolvoidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_devinfo_matches'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='bool'; fcns.RHS{fcnNum}={'bladerf_devinfoPtr', 'bladerf_devinfoPtr'};fcnNum=fcnNum+1;

% _Bool bladerf_devstr_matches ( const char * dev_str , struct bladerf_devinfo * info );
fcns.thunkname{fcnNum}='_BoolcstringvoidPtrThunk';fcns.name{fcnNum}='bladerf_devstr_matches'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='bool'; fcns.RHS{fcnNum}={'cstring', 'bladerf_devinfoPtr'};fcnNum=fcnNum+1;

% int bladerf_open_with_devinfo ( struct bladerf ** device , struct bladerf_devinfo * devinfo );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_open_with_devinfo'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtrPtr', 'bladerf_devinfoPtr'};fcnNum=fcnNum+1;

% int bladerf_open ( struct bladerf ** device , const char * device_identifier );
fcns.thunkname{fcnNum}='int32voidPtrcstringThunk';fcns.name{fcnNum}='bladerf_open'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtrPtr', 'cstring'};fcnNum=fcnNum+1;

% void bladerf_close ( struct bladerf * device );
fcns.thunkname{fcnNum}='voidvoidPtrThunk';fcns.name{fcnNum}='bladerf_close'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={'bladerfPtr'};fcnNum=fcnNum+1;

% void bladerf_set_usb_reset_on_open ( _Bool enabled );
fcns.thunkname{fcnNum}='void_BoolThunk';fcns.name{fcnNum}='bladerf_set_usb_reset_on_open'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={'bool'};fcnNum=fcnNum+1;

% int bladerf_get_serial ( struct bladerf * dev , char * serial );
fcns.thunkname{fcnNum}='int32voidPtrcstringThunk';fcns.name{fcnNum}='bladerf_get_serial'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'cstring'};fcnNum=fcnNum+1;

% int bladerf_get_vctcxo_trim ( struct bladerf * dev , uint16_t * trim );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_vctcxo_trim'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint16Ptr'};fcnNum=fcnNum+1;

% int bladerf_get_fpga_size ( struct bladerf * dev , bladerf_fpga_size * size );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_fpga_size'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_fpga_sizePtr'};fcnNum=fcnNum+1;

% int bladerf_fw_version ( struct bladerf * dev , struct bladerf_version * version );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_fw_version'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_versionPtr'};fcnNum=fcnNum+1;

% int bladerf_is_fpga_configured ( struct bladerf * dev );
fcns.thunkname{fcnNum}='int32voidPtrThunk';fcns.name{fcnNum}='bladerf_is_fpga_configured'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr'};fcnNum=fcnNum+1;

% int bladerf_fpga_version ( struct bladerf * dev , struct bladerf_version * version );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_fpga_version'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_versionPtr'};fcnNum=fcnNum+1;

% bladerf_dev_speed bladerf_device_speed ( struct bladerf * dev );
fcns.thunkname{fcnNum}='bladerf_dev_speedvoidPtrThunk';fcns.name{fcnNum}='bladerf_device_speed'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='bladerf_dev_speed'; fcns.RHS{fcnNum}={'bladerfPtr'};fcnNum=fcnNum+1;

% int bladerf_enable_module ( struct bladerf * dev , bladerf_channel ch , _Bool enable );
fcns.thunkname{fcnNum}='int32voidPtrint32_BoolThunk';fcns.name{fcnNum}='bladerf_enable_module'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bool'};fcnNum=fcnNum+1;

% int bladerf_set_txvga2 ( struct bladerf * dev , int gain );
fcns.thunkname{fcnNum}='int32voidPtrint32Thunk';fcns.name{fcnNum}='bladerf_set_txvga2'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'int32'};fcnNum=fcnNum+1;

% int bladerf_get_txvga2 ( struct bladerf * dev , int * gain );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_txvga2'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'int32Ptr'};fcnNum=fcnNum+1;

% int bladerf_set_txvga1 ( struct bladerf * dev , int gain );
fcns.thunkname{fcnNum}='int32voidPtrint32Thunk';fcns.name{fcnNum}='bladerf_set_txvga1'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'int32'};fcnNum=fcnNum+1;

% int bladerf_get_txvga1 ( struct bladerf * dev , int * gain );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_txvga1'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'int32Ptr'};fcnNum=fcnNum+1;

% int bladerf_set_lna_gain ( struct bladerf * dev , bladerf_lna_gain gain );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_lna_gainThunk';fcns.name{fcnNum}='bladerf_set_lna_gain'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_lna_gain'};fcnNum=fcnNum+1;

% int bladerf_get_lna_gain ( struct bladerf * dev , bladerf_lna_gain * gain );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_lna_gain'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_lna_gainPtr'};fcnNum=fcnNum+1;

% int bladerf_set_rxvga1 ( struct bladerf * dev , int gain );
fcns.thunkname{fcnNum}='int32voidPtrint32Thunk';fcns.name{fcnNum}='bladerf_set_rxvga1'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'int32'};fcnNum=fcnNum+1;

% int bladerf_get_rxvga1 ( struct bladerf * dev , int * gain );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_rxvga1'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'int32Ptr'};fcnNum=fcnNum+1;

% int bladerf_set_rxvga2 ( struct bladerf * dev , int gain );
fcns.thunkname{fcnNum}='int32voidPtrint32Thunk';fcns.name{fcnNum}='bladerf_set_rxvga2'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'int32'};fcnNum=fcnNum+1;

% int bladerf_get_rxvga2 ( struct bladerf * dev , int * gain );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_rxvga2'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'int32Ptr'};fcnNum=fcnNum+1;

% int bladerf_set_gain ( struct bladerf * dev , bladerf_channel ch , bladerf_gain gain );
fcns.thunkname{fcnNum}='int32voidPtrint32int32Thunk';fcns.name{fcnNum}='bladerf_set_gain'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'int32'};fcnNum=fcnNum+1;

% int bladerf_set_gain_mode ( struct bladerf * dev , bladerf_channel ch , bladerf_gain_mode mode );
fcns.thunkname{fcnNum}='int32voidPtrint32bladerf_gain_modeThunk';fcns.name{fcnNum}='bladerf_set_gain_mode'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_gain_mode'};fcnNum=fcnNum+1;

% int bladerf_get_gain_mode ( struct bladerf * dev , bladerf_channel ch , bladerf_gain_mode * mode );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_gain_mode'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_gain_modePtr'};fcnNum=fcnNum+1;

% int bladerf_set_sample_rate ( struct bladerf * dev , bladerf_channel ch , bladerf_sample_rate rate , bladerf_sample_rate * actual );
fcns.thunkname{fcnNum}='int32voidPtrint32uint32voidPtrThunk';fcns.name{fcnNum}='bladerf_set_sample_rate'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'uint32', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_set_rational_sample_rate ( struct bladerf * dev , bladerf_channel ch , struct bladerf_rational_rate * rate , struct bladerf_rational_rate * actual );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_set_rational_sample_rate'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_rational_ratePtr', 'bladerf_rational_ratePtr'};fcnNum=fcnNum+1;

% int bladerf_get_sample_rate ( struct bladerf * dev , bladerf_channel ch , bladerf_sample_rate * rate );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_sample_rate'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_get_rational_sample_rate ( struct bladerf * dev , bladerf_channel ch , struct bladerf_rational_rate * rate );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_rational_sample_rate'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_rational_ratePtr'};fcnNum=fcnNum+1;

% int bladerf_set_sampling ( struct bladerf * dev , bladerf_sampling sampling );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_samplingThunk';fcns.name{fcnNum}='bladerf_set_sampling'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_sampling'};fcnNum=fcnNum+1;

% int bladerf_set_rx_mux ( struct bladerf * dev , bladerf_rx_mux mux );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_rx_muxThunk';fcns.name{fcnNum}='bladerf_set_rx_mux'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_rx_mux'};fcnNum=fcnNum+1;

% int bladerf_get_rx_mux ( struct bladerf * dev , bladerf_rx_mux * mode );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_rx_mux'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_rx_muxPtr'};fcnNum=fcnNum+1;

% int bladerf_get_sampling ( struct bladerf * dev , bladerf_sampling * sampling );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_sampling'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_samplingPtr'};fcnNum=fcnNum+1;

% int bladerf_set_bandwidth ( struct bladerf * dev , bladerf_channel ch , bladerf_bandwidth bandwidth , bladerf_bandwidth * actual );
fcns.thunkname{fcnNum}='int32voidPtrint32uint32voidPtrThunk';fcns.name{fcnNum}='bladerf_set_bandwidth'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'uint32', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_get_bandwidth ( struct bladerf * dev , bladerf_channel ch , bladerf_bandwidth * bandwidth );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_bandwidth'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_set_lpf_mode ( struct bladerf * dev , bladerf_channel ch , bladerf_lpf_mode mode );
fcns.thunkname{fcnNum}='int32voidPtrint32bladerf_lpf_modeThunk';fcns.name{fcnNum}='bladerf_set_lpf_mode'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_lpf_mode'};fcnNum=fcnNum+1;

% int bladerf_get_lpf_mode ( struct bladerf * dev , bladerf_channel ch , bladerf_lpf_mode * mode );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_lpf_mode'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_lpf_modePtr'};fcnNum=fcnNum+1;

% int bladerf_select_band ( struct bladerf * dev , bladerf_channel ch , bladerf_frequency frequency );
fcns.thunkname{fcnNum}=['int32voidPtrint32' u64_type 'Thunk'];fcns.name{fcnNum}='bladerf_select_band'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', u64_type};fcnNum=fcnNum+1;

% int bladerf_set_frequency ( struct bladerf * dev , bladerf_channel ch , bladerf_frequency frequency );
fcns.thunkname{fcnNum}=['int32voidPtrint32' u64_type 'Thunk'];fcns.name{fcnNum}='bladerf_set_frequency'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', u64_type};fcnNum=fcnNum+1;

% int bladerf_schedule_retune ( struct bladerf * dev , bladerf_channel ch , bladerf_timestamp timestamp , bladerf_frequency frequency , struct bladerf_quick_tune * quick_tune );
fcns.thunkname{fcnNum}=['int32voidPtrint32' u64_type u64_type 'voidPtrThunk'];fcns.name{fcnNum}='bladerf_schedule_retune'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', u64_type, u64_type, 'voidPtr'};fcnNum=fcnNum+1;

% int bladerf_cancel_scheduled_retunes ( struct bladerf * dev , bladerf_channel ch );
fcns.thunkname{fcnNum}='int32voidPtrint32Thunk';fcns.name{fcnNum}='bladerf_cancel_scheduled_retunes'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel'};fcnNum=fcnNum+1;

% int bladerf_get_frequency ( struct bladerf * dev , bladerf_channel ch , bladerf_frequency * frequency );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_frequency'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', [u64_type 'Ptr'] };fcnNum=fcnNum+1;

% int bladerf_get_quick_tune ( struct bladerf * dev , bladerf_channel ch , struct bladerf_quick_tune * quick_tune );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_quick_tune'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'voidPtr'};fcnNum=fcnNum+1;

% int bladerf_set_tuning_mode ( struct bladerf * dev , bladerf_tuning_mode mode );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_tuning_modeThunk';fcns.name{fcnNum}='bladerf_set_tuning_mode'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_tuning_mode'};fcnNum=fcnNum+1;

% int bladerf_set_loopback ( struct bladerf * dev , bladerf_loopback lb );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_loopbackThunk';fcns.name{fcnNum}='bladerf_set_loopback'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_loopback'};fcnNum=fcnNum+1;

% int bladerf_get_loopback ( struct bladerf * dev , bladerf_loopback * lb );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_loopback'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_loopbackPtr'};fcnNum=fcnNum+1;

% int bladerf_set_smb_mode ( struct bladerf * dev , bladerf_smb_mode mode );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_smb_modeThunk';fcns.name{fcnNum}='bladerf_set_smb_mode'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_smb_mode'};fcnNum=fcnNum+1;

% int bladerf_get_smb_mode ( struct bladerf * dev , bladerf_smb_mode * mode );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_smb_mode'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_smb_modePtr'};fcnNum=fcnNum+1;

% int bladerf_set_rational_smb_frequency ( struct bladerf * dev , struct bladerf_rational_rate * rate , struct bladerf_rational_rate * actual );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_set_rational_smb_frequency'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_rational_ratePtr', 'bladerf_rational_ratePtr'};fcnNum=fcnNum+1;

% int bladerf_set_smb_frequency ( struct bladerf * dev , uint32_t rate , uint32_t * actual );
fcns.thunkname{fcnNum}='int32voidPtruint32voidPtrThunk';fcns.name{fcnNum}='bladerf_set_smb_frequency'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_get_rational_smb_frequency ( struct bladerf * dev , struct bladerf_rational_rate * rate );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_rational_smb_frequency'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_rational_ratePtr'};fcnNum=fcnNum+1;

% int bladerf_get_smb_frequency ( struct bladerf * dev , unsigned int * rate );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_smb_frequency'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_trigger_init ( struct bladerf * dev , bladerf_channel ch , bladerf_trigger_signal signal , struct bladerf_trigger * trigger );
fcns.thunkname{fcnNum}='int32voidPtrint32bladerf_trigger_signalvoidPtrThunk';fcns.name{fcnNum}='bladerf_trigger_init'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_trigger_signal', 'bladerf_triggerPtr'};fcnNum=fcnNum+1;

% int bladerf_trigger_arm ( struct bladerf * dev , const struct bladerf_trigger * trigger , _Bool arm , uint64_t resv1 , uint64_t resv2 );
fcns.thunkname{fcnNum}=['int32voidPtrvoidPtr_Bool' u64_type u64_type 'Thunk'];fcns.name{fcnNum}='bladerf_trigger_arm'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_triggerPtr', 'bool', u64_type, u64_type};fcnNum=fcnNum+1;

% int bladerf_trigger_fire ( struct bladerf * dev , const struct bladerf_trigger * trigger );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_trigger_fire'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_triggerPtr'};fcnNum=fcnNum+1;

% int bladerf_trigger_state ( struct bladerf * dev , const struct bladerf_trigger * trigger , _Bool * is_armed , _Bool * has_fired , _Bool * fire_requested , uint64_t * resv1 , uint64_t * resv2 );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrvoidPtrvoidPtrvoidPtrvoidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_trigger_state'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_triggerPtr', 'boolPtr', 'boolPtr', 'boolPtr', [u64_type 'Ptr'], [u64_type 'Ptr']};fcnNum=fcnNum+1;

% int bladerf_set_correction ( struct bladerf * dev , bladerf_channel ch , bladerf_correction corr , bladerf_correction_value value );
fcns.thunkname{fcnNum}='int32voidPtrint32bladerf_correctionint16Thunk';fcns.name{fcnNum}='bladerf_set_correction'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_correction', 'int16'};fcnNum=fcnNum+1;

% int bladerf_get_correction ( struct bladerf * dev , bladerf_channel ch , bladerf_correction corr , bladerf_correction_value * value );
fcns.thunkname{fcnNum}='int32voidPtrint32bladerf_correctionvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_correction'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_correction', 'int16Ptr'};fcnNum=fcnNum+1;

% int bladerf_set_vctcxo_tamer_mode ( struct bladerf * dev , bladerf_vctcxo_tamer_mode mode );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_vctcxo_tamer_modeThunk';fcns.name{fcnNum}='bladerf_set_vctcxo_tamer_mode'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_vctcxo_tamer_mode'};fcnNum=fcnNum+1;

% int bladerf_get_vctcxo_tamer_mode ( struct bladerf * dev , bladerf_vctcxo_tamer_mode * mode );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_vctcxo_tamer_mode'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_vctcxo_tamer_modePtr'};fcnNum=fcnNum+1;

% int bladerf_dac_write ( struct bladerf * dev , uint16_t val );
fcns.thunkname{fcnNum}='int32voidPtruint16Thunk';fcns.name{fcnNum}='bladerf_dac_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint16'};fcnNum=fcnNum+1;

% int bladerf_dac_read ( struct bladerf * dev , uint16_t * val );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_dac_read'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint16Ptr'};fcnNum=fcnNum+1;

% int bladerf_expansion_attach ( struct bladerf * dev , bladerf_xb xb );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_xbThunk';fcns.name{fcnNum}='bladerf_expansion_attach'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_xb'};fcnNum=fcnNum+1;

% int bladerf_expansion_get_attached ( struct bladerf * dev , bladerf_xb * xb );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_expansion_get_attached'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_xbPtr'};fcnNum=fcnNum+1;

% int bladerf_xb200_set_filterbank ( struct bladerf * dev , bladerf_channel ch , bladerf_xb200_filter filter );
fcns.thunkname{fcnNum}='int32voidPtrint32bladerf_xb200_filterThunk';fcns.name{fcnNum}='bladerf_xb200_set_filterbank'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_xb200_filter'};fcnNum=fcnNum+1;

% int bladerf_xb200_get_filterbank ( struct bladerf * dev , bladerf_channel ch , bladerf_xb200_filter * filter );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_xb200_get_filterbank'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_xb200_filterPtr'};fcnNum=fcnNum+1;

% int bladerf_xb200_set_path ( struct bladerf * dev , bladerf_channel ch , bladerf_xb200_path path );
fcns.thunkname{fcnNum}='int32voidPtrint32bladerf_xb200_pathThunk';fcns.name{fcnNum}='bladerf_xb200_set_path'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_xb200_path'};fcnNum=fcnNum+1;

% int bladerf_xb200_get_path ( struct bladerf * dev , bladerf_channel ch , bladerf_xb200_path * path );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_xb200_get_path'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_xb200_pathPtr'};fcnNum=fcnNum+1;

% int bladerf_xb300_set_trx ( struct bladerf * dev , bladerf_xb300_trx trx );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_xb300_trxThunk';fcns.name{fcnNum}='bladerf_xb300_set_trx'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_xb300_trx'};fcnNum=fcnNum+1;

% int bladerf_xb300_get_trx ( struct bladerf * dev , bladerf_xb300_trx * trx );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_xb300_get_trx'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_xb300_trxPtr'};fcnNum=fcnNum+1;

% int bladerf_xb300_set_amplifier_enable ( struct bladerf * dev , bladerf_xb300_amplifier amp , _Bool enable );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_xb300_amplifier_BoolThunk';fcns.name{fcnNum}='bladerf_xb300_set_amplifier_enable'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_xb300_amplifier', 'bool'};fcnNum=fcnNum+1;

% int bladerf_xb300_get_amplifier_enable ( struct bladerf * dev , bladerf_xb300_amplifier amp , _Bool * enable );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_xb300_amplifiervoidPtrThunk';fcns.name{fcnNum}='bladerf_xb300_get_amplifier_enable'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_xb300_amplifier', 'boolPtr'};fcnNum=fcnNum+1;

% int bladerf_xb300_get_output_power ( struct bladerf * dev , float * val );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_xb300_get_output_power'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'singlePtr'};fcnNum=fcnNum+1;

% int bladerf_expansion_gpio_read ( struct bladerf * dev , uint32_t * val );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_expansion_gpio_read'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_expansion_gpio_write ( struct bladerf * dev , uint32_t val );
fcns.thunkname{fcnNum}='int32voidPtruint32Thunk';fcns.name{fcnNum}='bladerf_expansion_gpio_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_expansion_gpio_masked_write ( struct bladerf * dev , uint32_t mask , uint32_t value );
fcns.thunkname{fcnNum}='int32voidPtruint32uint32Thunk';fcns.name{fcnNum}='bladerf_expansion_gpio_masked_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_expansion_gpio_dir_read ( struct bladerf * dev , uint32_t * outputs );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_expansion_gpio_dir_read'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_expansion_gpio_dir_write ( struct bladerf * dev , uint32_t outputs );
fcns.thunkname{fcnNum}='int32voidPtruint32Thunk';fcns.name{fcnNum}='bladerf_expansion_gpio_dir_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_expansion_gpio_dir_masked_write ( struct bladerf * dev , uint32_t mask , uint32_t outputs );
fcns.thunkname{fcnNum}='int32voidPtruint32uint32Thunk';fcns.name{fcnNum}='bladerf_expansion_gpio_dir_masked_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32', 'uint32'};fcnNum=fcnNum+1;

% const char * bladerf_backend_str ( bladerf_backend backend );
fcns.thunkname{fcnNum}='cstringbladerf_backendThunk';fcns.name{fcnNum}='bladerf_backend_str'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='cstring'; fcns.RHS{fcnNum}={'bladerf_backend'};fcnNum=fcnNum+1;

% void bladerf_version ( struct bladerf_version * version );
fcns.thunkname{fcnNum}='voidvoidPtrThunk';fcns.name{fcnNum}='bladerf_version'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={'bladerf_versionPtr'};fcnNum=fcnNum+1;

% void bladerf_log_set_verbosity ( bladerf_log_level level );
fcns.thunkname{fcnNum}='voidbladerf_log_levelThunk';fcns.name{fcnNum}='bladerf_log_set_verbosity'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={'bladerf_log_level'};fcnNum=fcnNum+1;

% int bladerf_get_fw_log ( struct bladerf * dev , const char * filename );
fcns.thunkname{fcnNum}='int32voidPtrcstringThunk';fcns.name{fcnNum}='bladerf_get_fw_log'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'cstring'};fcnNum=fcnNum+1;

% int bladerf_get_timestamp ( struct bladerf * dev , bladerf_direction dir , bladerf_timestamp * timestamp );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_directionvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_timestamp'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_direction', [u64_type 'Ptr']};fcnNum=fcnNum+1;

% int bladerf_init_stream ( struct bladerf_stream ** stream , struct bladerf * dev , bladerf_stream_cb callback , void *** buffers , size_t num_buffers , bladerf_format format , size_t samples_per_buffer , size_t num_transfers , void * user_data );
% NOTE: Matlab does not support FcnPtr or voidPtrPtrPtr (last checked: R2017b)
%fcns.thunkname{fcnNum}=['int32voidPtrvoidPtrvoidPtrvoidPtr' u64_type 'bladerf_format' u64_type u64_type 'voidPtrThunk'];fcns.name{fcnNum}='bladerf_init_stream'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerf_streamPtrPtr', 'bladerfPtr', 'FcnPtr', 'voidPtrPtrPtr', u64_type, 'bladerf_format', u64_type, u64_type, 'voidPtr'};fcnNum=fcnNum+1;

% int bladerf_stream ( struct bladerf_stream * stream , bladerf_channel_layout layout );
% NOTE: cannot bladerf_init_stream, so this function is unusable
%fcns.thunkname{fcnNum}='int32voidPtrbladerf_channel_layoutThunk';fcns.name{fcnNum}='bladerf_stream'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerf_streamPtr', 'bladerf_channel'};fcnNum=fcnNum+1;

% int bladerf_submit_stream_buffer ( struct bladerf_stream * stream , void * buffer , unsigned int timeout_ms );
% NOTE: cannot bladerf_init_stream, so this function is unusable
%fcns.thunkname{fcnNum}='int32voidPtrvoidPtruint32Thunk';fcns.name{fcnNum}='bladerf_submit_stream_buffer'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerf_streamPtr', 'voidPtr', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_submit_stream_buffer_nb ( struct bladerf_stream * stream , void * buffer );
% NOTE: cannot bladerf_init_stream, so this function is unusable
%fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_submit_stream_buffer_nb'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerf_streamPtr', 'voidPtr'};fcnNum=fcnNum+1;

% void bladerf_deinit_stream ( struct bladerf_stream * stream );
% NOTE: cannot bladerf_init_stream, so this function is unusable
%fcns.thunkname{fcnNum}='voidvoidPtrThunk';fcns.name{fcnNum}='bladerf_deinit_stream'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={'bladerf_streamPtr'};fcnNum=fcnNum+1;

% int bladerf_set_stream_timeout ( struct bladerf * dev , bladerf_direction dir , unsigned int timeout );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_directionuint32Thunk';fcns.name{fcnNum}='bladerf_set_stream_timeout'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_direction', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_get_stream_timeout ( struct bladerf * dev , bladerf_direction dir , unsigned int * timeout );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_directionvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_stream_timeout'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_direction', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_sync_config ( struct bladerf * dev , bladerf_channel_layout layout , bladerf_format format , unsigned int num_buffers , unsigned int buffer_size , unsigned int num_transfers , unsigned int stream_timeout );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_channel_layoutbladerf_formatuint32uint32uint32uint32Thunk';fcns.name{fcnNum}='bladerf_sync_config'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_format', 'uint32', 'uint32', 'uint32', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_sync_tx ( struct bladerf * dev , const void * samples , unsigned int num_samples , struct bladerf_metadata * metadata , unsigned int timeout_ms );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtruint32voidPtruint32Thunk';fcns.name{fcnNum}='bladerf_sync_tx'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'voidPtr', 'uint32', 'bladerf_metadataPtr', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_sync_rx ( struct bladerf * dev , void * samples , unsigned int num_samples , struct bladerf_metadata * metadata , unsigned int timeout_ms );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtruint32voidPtruint32Thunk';fcns.name{fcnNum}='bladerf_sync_rx'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'voidPtr', 'uint32', 'bladerf_metadataPtr', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_flash_firmware ( struct bladerf * dev , const char * firmware );
fcns.thunkname{fcnNum}='int32voidPtrcstringThunk';fcns.name{fcnNum}='bladerf_flash_firmware'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'cstring'};fcnNum=fcnNum+1;

% int bladerf_load_fpga ( struct bladerf * dev , const char * fpga );
fcns.thunkname{fcnNum}='int32voidPtrcstringThunk';fcns.name{fcnNum}='bladerf_load_fpga'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'cstring'};fcnNum=fcnNum+1;

% int bladerf_flash_fpga ( struct bladerf * dev , const char * fpga_image );
fcns.thunkname{fcnNum}='int32voidPtrcstringThunk';fcns.name{fcnNum}='bladerf_flash_fpga'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'cstring'};fcnNum=fcnNum+1;

% int bladerf_erase_stored_fpga ( struct bladerf * dev );
fcns.thunkname{fcnNum}='int32voidPtrThunk';fcns.name{fcnNum}='bladerf_erase_stored_fpga'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr'};fcnNum=fcnNum+1;

% int bladerf_device_reset ( struct bladerf * dev );
fcns.thunkname{fcnNum}='int32voidPtrThunk';fcns.name{fcnNum}='bladerf_device_reset'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr'};fcnNum=fcnNum+1;

% int bladerf_jump_to_bootloader ( struct bladerf * dev );
fcns.thunkname{fcnNum}='int32voidPtrThunk';fcns.name{fcnNum}='bladerf_jump_to_bootloader'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr'};fcnNum=fcnNum+1;

% int bladerf_get_bootloader_list ( struct bladerf_devinfo ** list );
fcns.thunkname{fcnNum}='int32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_bootloader_list'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerf_devinfoPtrPtr'};fcnNum=fcnNum+1;

% int bladerf_load_fw_from_bootloader ( const char * device_identifier , bladerf_backend backend , uint8_t bus , uint8_t addr , const char * file );
fcns.thunkname{fcnNum}='int32cstringbladerf_backenduint8uint8cstringThunk';fcns.name{fcnNum}='bladerf_load_fw_from_bootloader'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'cstring', 'bladerf_backend', 'uint8', 'uint8', 'cstring'};fcnNum=fcnNum+1;

% struct bladerf_image * bladerf_alloc_image ( struct bladerf * dev , bladerf_image_type type , uint32_t address , uint32_t length );
fcns.thunkname{fcnNum}='voidPtrvoidPtrbladerf_image_typeuint32uint32Thunk';fcns.name{fcnNum}='bladerf_alloc_image'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='bladerf_imagePtr'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_image_type', 'uint32', 'uint32'};fcnNum=fcnNum+1;

% struct bladerf_image * bladerf_alloc_cal_image ( struct bladerf * dev , bladerf_fpga_size fpga_size , uint16_t vctcxo_trim );
fcns.thunkname{fcnNum}='voidPtrvoidPtrbladerf_fpga_sizeuint16Thunk';fcns.name{fcnNum}='bladerf_alloc_cal_image'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='bladerf_imagePtr'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_fpga_size', 'uint16'};fcnNum=fcnNum+1;

% void bladerf_free_image ( struct bladerf_image * image );
fcns.thunkname{fcnNum}='voidvoidPtrThunk';fcns.name{fcnNum}='bladerf_free_image'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={'bladerf_imagePtr'};fcnNum=fcnNum+1;

% int bladerf_image_write ( struct bladerf * dev , struct bladerf_image * image , const char * file );
fcns.thunkname{fcnNum}='int32voidPtrcstringThunk';fcns.name{fcnNum}='bladerf_image_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_imagePtr', 'cstring'};fcnNum=fcnNum+1;

% int bladerf_image_read ( struct bladerf_image * image , const char * file );
fcns.thunkname{fcnNum}='int32voidPtrcstringThunk';fcns.name{fcnNum}='bladerf_image_read'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerf_imagePtr', 'cstring'};fcnNum=fcnNum+1;

% int bladerf_si5338_read ( struct bladerf * dev , uint8_t address , uint8_t * val );
fcns.thunkname{fcnNum}='int32voidPtruint8voidPtrThunk';fcns.name{fcnNum}='bladerf_si5338_read'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint8', 'uint8Ptr'};fcnNum=fcnNum+1;

% int bladerf_si5338_write ( struct bladerf * dev , uint8_t address , uint8_t val );
fcns.thunkname{fcnNum}='int32voidPtruint8uint8Thunk';fcns.name{fcnNum}='bladerf_si5338_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint8', 'uint8'};fcnNum=fcnNum+1;

% int bladerf_lms_read ( struct bladerf * dev , uint8_t address , uint8_t * val );
fcns.thunkname{fcnNum}='int32voidPtruint8voidPtrThunk';fcns.name{fcnNum}='bladerf_lms_read'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint8', 'uint8Ptr'};fcnNum=fcnNum+1;

% int bladerf_lms_write ( struct bladerf * dev , uint8_t address , uint8_t val );
fcns.thunkname{fcnNum}='int32voidPtruint8uint8Thunk';fcns.name{fcnNum}='bladerf_lms_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint8', 'uint8'};fcnNum=fcnNum+1;

% int bladerf_lms_set_dc_cals ( struct bladerf * dev , const struct bladerf_lms_dc_cals * dc_cals );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_lms_set_dc_cals'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_lms_dc_calsPtr'};fcnNum=fcnNum+1;

% int bladerf_lms_get_dc_cals ( struct bladerf * dev , struct bladerf_lms_dc_cals * dc_cals );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_lms_get_dc_cals'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_lms_dc_calsPtr'};fcnNum=fcnNum+1;

% int bladerf_config_gpio_read ( struct bladerf * dev , uint32_t * val );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_config_gpio_read'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_config_gpio_write ( struct bladerf * dev , uint32_t val );
fcns.thunkname{fcnNum}='int32voidPtruint32Thunk';fcns.name{fcnNum}='bladerf_config_gpio_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_xb_spi_write ( struct bladerf * dev , uint32_t val );
fcns.thunkname{fcnNum}='int32voidPtruint32Thunk';fcns.name{fcnNum}='bladerf_xb_spi_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_calibrate_dc ( struct bladerf * dev , bladerf_cal_module module );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_cal_moduleThunk';fcns.name{fcnNum}='bladerf_calibrate_dc'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_cal_module'};fcnNum=fcnNum+1;

% int bladerf_read_trigger ( struct bladerf * dev , bladerf_channel ch , bladerf_trigger_signal signal , uint8_t * val );
fcns.thunkname{fcnNum}='int32voidPtrint32bladerf_trigger_signalvoidPtrThunk';fcns.name{fcnNum}='bladerf_read_trigger'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_trigger_signal', 'uint8Ptr'};fcnNum=fcnNum+1;

% int bladerf_write_trigger ( struct bladerf * dev , bladerf_channel ch , bladerf_trigger_signal signal , uint8_t val );
fcns.thunkname{fcnNum}='int32voidPtrint32bladerf_trigger_signaluint8Thunk';fcns.name{fcnNum}='bladerf_write_trigger'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_trigger_signal', 'uint8'};fcnNum=fcnNum+1;

% int bladerf_erase_flash ( struct bladerf * dev , uint32_t erase_block , uint32_t count );
fcns.thunkname{fcnNum}='int32voidPtruint32uint32Thunk';fcns.name{fcnNum}='bladerf_erase_flash'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_read_flash ( struct bladerf * dev , uint8_t * buf , uint32_t page , uint32_t count );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtruint32uint32Thunk';fcns.name{fcnNum}='bladerf_read_flash'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint8Ptr', 'uint32', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_write_flash ( struct bladerf * dev , const uint8_t * buf , uint32_t page , uint32_t count );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtruint32uint32Thunk';fcns.name{fcnNum}='bladerf_write_flash'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint8Ptr', 'uint32', 'uint32'};fcnNum=fcnNum+1;

% const char * bladerf_strerror ( int error );
fcns.thunkname{fcnNum}='cstringint32Thunk';fcns.name{fcnNum}='bladerf_strerror'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='cstring'; fcns.RHS{fcnNum}={'int32'};fcnNum=fcnNum+1;

% const char * bladerf_get_board_name ( struct bladerf * dev );
fcns.thunkname{fcnNum}='cstringvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_board_name'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='cstring'; fcns.RHS{fcnNum}={'bladerfPtr'};fcnNum=fcnNum+1;

% size_t bladerf_get_channel_count ( struct bladerf * dev , bladerf_direction dir );
fcns.thunkname{fcnNum}=[ u64_type 'voidPtrbladerf_directionThunk'];fcns.name{fcnNum}='bladerf_get_channel_count'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=u64_type; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_direction'};fcnNum=fcnNum+1;

% int bladerf_get_gain ( struct bladerf * dev , bladerf_channel ch , bladerf_gain * gain );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_gain'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'int32Ptr'};fcnNum=fcnNum+1;

% int bladerf_get_gain_modes ( struct bladerf * dev , bladerf_channel ch , const struct bladerf_gain_modes ** modes );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_gain_modes'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_gain_modesPtrPtr'};fcnNum=fcnNum+1;

% int bladerf_get_gain_range ( struct bladerf * dev , bladerf_channel ch , const struct bladerf_range ** range );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_gain_range'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_rangePtrPtr'};fcnNum=fcnNum+1;

% int bladerf_set_gain_stage ( struct bladerf * dev , bladerf_channel ch , const char * stage , bladerf_gain gain );
fcns.thunkname{fcnNum}='int32voidPtrint32cstringint32Thunk';fcns.name{fcnNum}='bladerf_set_gain_stage'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'cstring', 'int32'};fcnNum=fcnNum+1;

% int bladerf_get_gain_stage ( struct bladerf * dev , bladerf_channel ch , const char * stage , bladerf_gain * gain );
fcns.thunkname{fcnNum}='int32voidPtrint32cstringvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_gain_stage'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'cstring', 'int32Ptr'};fcnNum=fcnNum+1;

% int bladerf_get_gain_stage_range ( struct bladerf * dev , bladerf_channel ch , const char * stage , const struct bladerf_range ** range );
fcns.thunkname{fcnNum}='int32voidPtrint32cstringvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_gain_stage_range'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'cstring', 'bladerf_rangePtrPtr'};fcnNum=fcnNum+1;

% int bladerf_get_gain_stages ( struct bladerf * dev , bladerf_channel ch , const char ** stages , size_t count );
fcns.thunkname{fcnNum}=['int32voidPtrint32voidPtr' u64_type 'Thunk'];fcns.name{fcnNum}='bladerf_get_gain_stages'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'stringPtrPtr', u64_type};fcnNum=fcnNum+1;

% int bladerf_get_sample_rate_range ( struct bladerf * dev , bladerf_channel ch , const struct bladerf_range ** range );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_sample_rate_range'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_rangePtrPtr'};fcnNum=fcnNum+1;

% int bladerf_get_bandwidth_range ( struct bladerf * dev , bladerf_channel ch , const struct bladerf_range ** range );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_bandwidth_range'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_rangePtrPtr'};fcnNum=fcnNum+1;

% int bladerf_get_frequency_range ( struct bladerf * dev , bladerf_channel ch , const struct bladerf_range ** range );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_frequency_range'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bladerf_rangePtrPtr'};fcnNum=fcnNum+1;

% int bladerf_set_rf_port ( struct bladerf * dev , bladerf_channel ch , const char * port );
fcns.thunkname{fcnNum}='int32voidPtrint32cstringThunk';fcns.name{fcnNum}='bladerf_set_rf_port'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'cstring'};fcnNum=fcnNum+1;

% int bladerf_get_rf_port ( struct bladerf * dev , bladerf_channel ch , const char ** port );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_rf_port'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'stringPtrPtr'};fcnNum=fcnNum+1;

% int bladerf_get_rf_ports ( struct bladerf * dev , bladerf_channel ch , const char ** ports , unsigned int count );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtruint32Thunk';fcns.name{fcnNum}='bladerf_get_rf_ports'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'stringPtrPtr', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_get_loopback_modes ( struct bladerf * dev , const struct bladerf_loopback_modes ** modes );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_loopback_modes'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_loopback_modesPtrPtr'};fcnNum=fcnNum+1;

%  _Bool bladerf_is_loopback_mode_supported ( struct bladerf * dev , bladerf_loopback mode );
fcns.thunkname{fcnNum}='_BoolvoidPtrbladerf_loopbackThunk';fcns.name{fcnNum}='bladerf_is_loopback_mode_supported'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='bool'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_loopback'};fcnNum=fcnNum+1;

% int bladerf_interleave_stream_buffer ( bladerf_channel_layout layout , bladerf_format format , unsigned int buffer_size , void * samples );
fcns.thunkname{fcnNum}='int32bladerf_channel_layoutbladerf_formatuint32voidPtrThunk';fcns.name{fcnNum}='bladerf_interleave_stream_buffer'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerf_channel', 'bladerf_format', 'uint32', 'voidPtr'};fcnNum=fcnNum+1;

% int bladerf_deinterleave_stream_buffer ( bladerf_channel_layout layout , bladerf_format format , unsigned int buffer_size , void * samples );
fcns.thunkname{fcnNum}='int32bladerf_channel_layoutbladerf_formatuint32voidPtrThunk';fcns.name{fcnNum}='bladerf_deinterleave_stream_buffer'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerf_channel', 'bladerf_format', 'uint32', 'voidPtr'};fcnNum=fcnNum+1;

% int bladerf_trim_dac_write ( struct bladerf * dev , uint16_t val );
fcns.thunkname{fcnNum}='int32voidPtruint16Thunk';fcns.name{fcnNum}='bladerf_trim_dac_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint16'};fcnNum=fcnNum+1;

% int bladerf_trim_dac_read ( struct bladerf * dev , uint16_t * val );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_trim_dac_read'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint16Ptr'};fcnNum=fcnNum+1;

% int bladerf_get_tuning_mode ( struct bladerf * dev , bladerf_tuning_mode * mode );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_tuning_mode'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_tuning_modePtr'};fcnNum=fcnNum+1;

% int bladerf_get_bias_tee ( struct bladerf * dev , bladerf_channel ch , _Bool * enable );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_bias_tee'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'boolPtr'};fcnNum=fcnNum+1;

% int bladerf_set_bias_tee ( struct bladerf * dev , bladerf_channel ch , _Bool enable );
fcns.thunkname{fcnNum}='int32voidPtrint32_BoolThunk';fcns.name{fcnNum}='bladerf_set_bias_tee'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'bool'};fcnNum=fcnNum+1;

% int bladerf_get_rfic_register ( struct bladerf * dev , uint16_t address , uint8_t * val );
fcns.thunkname{fcnNum}='int32voidPtruint16voidPtrThunk';fcns.name{fcnNum}='bladerf_get_rfic_register'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint16', 'uint8Ptr'};fcnNum=fcnNum+1;

% int bladerf_set_rfic_register ( struct bladerf * dev , uint16_t address , uint8_t val );
fcns.thunkname{fcnNum}='int32voidPtruint16uint8Thunk';fcns.name{fcnNum}='bladerf_set_rfic_register'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint16', 'uint8'};fcnNum=fcnNum+1;

% int bladerf_get_rfic_temperature ( struct bladerf * dev , float * val );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_rfic_temperature'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'singlePtr'};fcnNum=fcnNum+1;

% int bladerf_get_rfic_rssi ( struct bladerf * dev , bladerf_channel ch , int32_t * pre_rssi , int32_t * sym_rssi );
fcns.thunkname{fcnNum}='int32voidPtrint32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_rfic_rssi'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_channel', 'int32Ptr', 'int32Ptr'};fcnNum=fcnNum+1;

% int bladerf_get_rfic_ctrl_out ( struct bladerf * dev , uint8_t * ctrl_out );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_rfic_ctrl_out'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint8Ptr'};fcnNum=fcnNum+1;

% int bladerf_get_pll_lock_state ( struct bladerf * dev , _Bool * locked );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_pll_lock_state'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'boolPtr'};fcnNum=fcnNum+1;

% int bladerf_get_pll_enable ( struct bladerf * dev , _Bool * enabled );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_pll_enable'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'boolPtr'};fcnNum=fcnNum+1;

% int bladerf_set_pll_enable ( struct bladerf * dev , _Bool enable );
fcns.thunkname{fcnNum}='int32voidPtr_BoolThunk';fcns.name{fcnNum}='bladerf_set_pll_enable'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bool'};fcnNum=fcnNum+1;

% int bladerf_get_pll_refclk_range ( struct bladerf * dev , const struct bladerf_range ** range );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_pll_refclk_range'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_rangePtrPtr'};fcnNum=fcnNum+1;

% int bladerf_get_pll_refclk ( struct bladerf * dev , uint64_t * frequency );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_pll_refclk'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', [ u64_type 'Ptr']};fcnNum=fcnNum+1;

% int bladerf_set_pll_refclk ( struct bladerf * dev , uint64_t frequency );
fcns.thunkname{fcnNum}=['int32voidPtr' u64_type 'Thunk'];fcns.name{fcnNum}='bladerf_set_pll_refclk'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', u64_type };fcnNum=fcnNum+1;

% int bladerf_get_pll_register ( struct bladerf * dev , uint8_t address , uint32_t * val );
fcns.thunkname{fcnNum}='int32voidPtruint8voidPtrThunk';fcns.name{fcnNum}='bladerf_get_pll_register'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint8', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_set_pll_register ( struct bladerf * dev , uint8_t address , uint32_t val );
fcns.thunkname{fcnNum}='int32voidPtruint8uint32Thunk';fcns.name{fcnNum}='bladerf_set_pll_register'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint8', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_get_power_source ( struct bladerf * dev , bladerf_power_sources * val );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_power_source'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_power_sourcesPtr'};fcnNum=fcnNum+1;

% int bladerf_get_clock_select ( struct bladerf * dev , bladerf_clock_select * sel );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_clock_select'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_clock_selectPtr'};fcnNum=fcnNum+1;

% int bladerf_set_clock_select ( struct bladerf * dev , bladerf_clock_select sel );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_clock_selectThunk';fcns.name{fcnNum}='bladerf_set_clock_select'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_clock_select'};fcnNum=fcnNum+1;

% int bladerf_get_clock_output ( struct bladerf * dev , _Bool * state );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_clock_output'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'boolPtr'};fcnNum=fcnNum+1;

% int bladerf_set_clock_output ( struct bladerf * dev , _Bool enable );
fcns.thunkname{fcnNum}='int32voidPtr_BoolThunk';fcns.name{fcnNum}='bladerf_set_clock_output'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bool'};fcnNum=fcnNum+1;

% int bladerf_get_pmic_register ( struct bladerf * dev , bladerf_pmic_register reg , void * val );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_pmic_registervoidPtrThunk';fcns.name{fcnNum}='bladerf_get_pmic_register'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_pmic_register', 'singlePtr'};fcnNum=fcnNum+1;

% int bladerf_get_rf_switch_config ( struct bladerf * dev , bladerf_rf_switch_config * config );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_rf_switch_config'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_rf_switch_configPtr'};fcnNum=fcnNum+1;


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Structures
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

structs.bladerf.members=struct('');

structs.bladerf_devinfo.members=struct('backend',  'bladerf_backend', ...
                                       'serial',   'int8#33', ...
                                       'usb_bus',  'uint8', ...
                                       'usb_addr', 'uint8', ...
                                       'instance', 'uint32', ...
                                       'manufacturer', 'int8#33', ...
                                       'product', 'int8#33');

structs.bladerf_version.members=struct('major',    'uint16', ...
                                       'minor',    'uint16', ...
                                       'patch',    'uint16', ...
                                       'describe', 'cstring');

structs.bladerf_rational_rate.members=struct('integer', u64_type, ...
                                             'num',     u64_type, ...
                                             'den',     u64_type);

structs.bladerf_quick_tune.members=struct('freqsel', 'uint8', ...
                                          'vcocap',  'uint8', ...
                                          'nint',    'uint16', ...
                                          'nfrac',   'uint32', ...
                                          'flags',   'uint8');

structs.bladerf_trigger.members=struct('channel',   'bladerf_channel', ...
                                       'role',      'bladerf_trigger_role', ...
                                       'signal',    'bladerf_trigger_signal', ...
                                       'options',   u64_type);

structs.bladerf_metadata.members=struct('timestamp',     u64_type, ...
                                        'flags',        'uint32', ...
                                        'status',       'uint32', ...
                                        'actual_count', 'uint32', ...
                                        'reserved',     'uint8#32');

% NOTE: cannot bladerf_init_stream, so this struct is unusable
%structs.bladerf_stream.members=struct('');

structs.bladerf_image.members=struct('magic',     'int8#8', ...
                                     'checksum',  'uint8#32', ...
                                     'version',   'bladerf_version', ...
                                     'timestamp',  u64_type, ...
                                     'serial',    'int8#34', ...
                                     'reserved',  'int8#128', ...
                                     'type',      'bladerf_image_type', ...
                                     'address',   'uint32', ...
                                     'length',    'uint32', ...
                                     'data',      'uint8Ptr');

structs.bladerf_lms_dc_cals.members=struct('lpf_tuning', 'int16', ...
                                           'tx_lpf_i',   'int16', ...
                                           'tx_lpf_q',   'int16', ...
                                           'rx_lpf_i',   'int16', ...
                                           'rx_lpf_q',   'int16', ...
                                           'dc_ref',     'int16', ...
                                           'rxvga2a_i',  'int16', ...
                                           'rxvga2a_q',  'int16', ...
                                           'rxvga2b_i',  'int16', ...
                                           'rxvga2b_q',  'int16');

structs.bladerf_range.members=struct('min',    'int64', ...
                                     'max',    'int64', ...
                                     'step',   'int64', ...
                                     'scale',  'single');

structs.bladerf_gain_modes.members=struct('name',   'cstring', ...
                                          'mode',   'bladerf_gain_mode');

structs.bladerf_loopback_modes.members=struct('name',   'cstring', ...
                                              'mode',   'bladerf_loopback');

structs.bladerf_rf_switch_config.members=struct('tx1_rfic_port', 'uint32', ...
                                                'tx1_spdt_port', 'uint32', ...
                                                'tx2_rfic_port', 'uint32', ...
                                                'tx2_spdt_port', 'uint32', ...
                                                'rx1_rfic_port', 'uint32', ...
                                                'rx1_spdt_port', 'uint32', ...
                                                'rx2_rfic_port', 'uint32', ...
                                                'rx2_spdt_port', 'uint32');

% Workaround for MATLAB <= 2015b (64-bit) not identifying the size of the devinfo structure correctly
structs.bladerf_devinfo.packing=4;


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Enumerations
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

enuminfo.bladerf_lpf_mode=struct('BLADERF_LPF_NORMAL',      0, ...
                                 'BLADERF_LPF_BYPASSED',    1, ...
                                 'BLADERF_LPF_DISABLED',    2);

enuminfo.bladerf_xb=struct('BLADERF_XB_NONE',   0, ...
                           'BLADERF_XB_100',    1, ...
                           'BLADERF_XB_200',    2, ...
                           'BLADERF_XB_300',    3);

enuminfo.bladerf_sampling=struct('BLADERF_SAMPLING_UNKNOWN',    0, ...
                                 'BLADERF_SAMPLING_INTERNAL',   1, ...
                                 'BLADERF_SAMPLING_EXTERNAL',   2);

enuminfo.bladerf_loopback=struct('BLADERF_LB_NONE',             0, ...
                                 'BLADERF_LB_FIRMWARE',         1, ...
                                 'BLADERF_LB_BB_TXLPF_RXVGA2',  2, ...
                                 'BLADERF_LB_BB_TXVGA1_RXVGA2', 3, ...
                                 'BLADERF_LB_BB_TXLPF_RXLPF',   4, ...
                                 'BLADERF_LB_BB_TXVGA1_RXLPF',  5, ...
                                 'BLADERF_LB_RF_LNA1',          6, ...
                                 'BLADERF_LB_RF_LNA2',          7, ...
                                 'BLADERF_LB_RF_LNA3',          8, ...
                                 'BLADERF_LB_RFIC_BIST',        9);

enuminfo.bladerf_log_level=struct('BLADERF_LOG_LEVEL_VERBOSE',  0, ...
                                  'BLADERF_LOG_LEVEL_DEBUG',    1, ...
                                  'BLADERF_LOG_LEVEL_INFO',     2, ...
                                  'BLADERF_LOG_LEVEL_WARNING',  3, ...
                                  'BLADERF_LOG_LEVEL_ERROR',    4, ...
                                  'BLADERF_LOG_LEVEL_CRITICAL', 5, ...
                                  'BLADERF_LOG_LEVEL_SILENT',   6);

enuminfo.bladerf_gain_mode=struct('BLADERF_GAIN_DEFAULT',        0, ...
                                  'BLADERF_GAIN_MGC',            1, ...
                                  'BLADERF_GAIN_FASTATTACK_AGC', 2, ...
                                  'BLADERF_GAIN_SLOWATTACK_AGC', 3, ...
                                  'BLADERF_GAIN_HYBRID_AGC',     4);

enuminfo.bladerf_format=struct('BLADERF_FORMAT_SC16_Q11',       0, ...
                               'BLADERF_FORMAT_SC16_Q11_META',  1);

enuminfo.bladerf_xb200_filter=struct('BLADERF_XB200_50M',       0, ...
                                     'BLADERF_XB200_144M',      1, ...
                                     'BLADERF_XB200_222M',      2, ...
                                     'BLADERF_XB200_CUSTOM',    3, ...
                                     'BLADERF_XB200_AUTO_1DB',  4, ...
                                     'BLADERF_XB200_AUTO_3DB',  5);

enuminfo.bladerf_fpga_size=struct('BLADERF_FPGA_UNKNOWN',   0,  ...
                                  'BLADERF_FPGA_40KLE',     40, ...
                                  'BLADERF_FPGA_115KLE',    115, ...
                                  'BLADERF_FPGA_A4',        49, ...
                                  'BLADERF_FPGA_A9',        301);

enuminfo.bladerf_xb300_trx=struct('BLADERF_XB300_TRX_INVAL',   -1, ...
                                  'BLADERF_XB300_TRX_TX',       0, ...
                                  'BLADERF_XB300_TRX_RX',       1, ...
                                  'BLADERF_XB300_TRX_UNSET',    2);

enuminfo.bladerf_xb300_amplifier=struct('BLADERF_XB300_AMP_INVAL', -1, ...
                                        'BLADERF_XB300_AMP_PA',     0, ...
                                        'BLADERF_XB300_AMP_LNA',    1, ...
                                        'BLADERF_XB300_AMP_PA_AUX', 2);

enuminfo.bladerf_rx_mux=struct('BLADERF_RX_MUX_INVALID',            -1, ...
                               'BLADERF_RX_MUX_BASEBAND',            0, ...
                               'BLADERF_RX_MUX_BASEBAND_LMS',        0, ...
                               'BLADERF_RX_MUX_12BIT_COUNTER',       1, ...
                               'BLADERF_RX_MUX_32BIT_COUNTER',       2, ...
                               'BLADERF_RX_MUX_DIGITAL_LOOPBACK',    4);
                               % Value 3 is reserved for future use

enuminfo.bladerf_dev_speed=struct('BLADERF_DEVICE_SPEED_UNKNOWN',   0, ...
                                  'BLADERF_DEVICE_SPEED_HIGH',      1, ...
                                  'BLADERF_DEVICE_SPEED_SUPER',     2);

enuminfo.bladerf_smb_mode=struct('BLADERF_SMB_MODE_INVALID',   -1, ...
                                 'BLADERF_SMB_MODE_DISABLED',   0, ...
                                 'BLADERF_SMB_MODE_OUTPUT',     1, ...
                                 'BLADERF_SMB_MODE_INPUT',      2, ...
                                 'BLADERF_SMB_MODE_UNAVAILBLE', 3);

enuminfo.bladerf_tuning_mode=struct('BLADERF_TUNING_MODE_INVALID', -1, ...
                                    'BLADERF_TUNING_MODE_HOST',     0, ...
                                    'BLADERF_TUNING_MODE_FPGA',     1);

enuminfo.bladerf_cal_module=struct('BLADERF_DC_CAL_INVALID',   -1, ...
                                   'BLADERF_DC_CAL_LPF_TUNING', 0, ...
                                   'BLADERF_DC_CAL_TX_LPF',     1, ...
                                   'BLADERF_DC_CAL_RX_LPF',     2, ...
                                   'BLADERF_DC_CAL_RXVGA2',     3);

enuminfo.bladerf_image_type=struct('BLADERF_IMAGE_TYPE_INVALID',       -1, ...
                                   'BLADERF_IMAGE_TYPE_RAW',            0, ...
                                   'BLADERF_IMAGE_TYPE_FIRMWARE',       1, ...
                                   'BLADERF_IMAGE_TYPE_FPGA_40KLE',     2, ...
                                   'BLADERF_IMAGE_TYPE_FPGA_115KLE',    3, ...
                                   'BLADERF_IMAGE_TYPE_CALIBRATION',    4, ...
                                   'BLADERF_IMAGE_TYPE_RX_DC_CAL',      5, ...
                                   'BLADERF_IMAGE_TYPE_TX_DC_CAL',      6, ...
                                   'BLADERF_IMAGE_TYPE_RX_IQ_CAL',      7, ...
                                   'BLADERF_IMAGE_TYPE_TX_IQ_CAL',      8);

enuminfo.bladerf_backend=struct('BLADERF_BACKEND_ANY',      0, ...
                                'BLADERF_BACKEND_LINUX',    1, ...
                                'BLADERF_BACKEND_LIBUSB',   2, ...
                                'BLADERF_BACKEND_CYPRESS',  3, ...
                                'BLADERF_BACKEND_DUMMY',    100);

enuminfo.bladerf_module=struct('BLADERF_MODULE_RX', 0, ...
                               'BLADERF_MODULE_TX', 1);

enuminfo.bladerf_channel=struct('BLADERF_MODULE_RX',  0, ...
                                'BLADERF_CHANNEL_RX1', 0, ...
                                'BLADERF_CHANNEL_RX2', 2, ...
                                'BLADERF_MODULE_TX',  1, ...
                                'BLADERF_CHANNEL_TX1', 1, ...
                                'BLADERF_CHANNEL_TX2', 3);

enuminfo.bladerf_vctcxo_tamer_mode=struct('BLADERF_VCTCXO_TAMER_INVALID',  -1, ...
                                          'BLADERF_VCTCXO_TAMER_DISABLED',  0, ...
                                          'BLADERF_VCTCXO_TAMER_1_PPS',     1, ...
                                          'BLADERF_VCTCXO_TAMER_10_MHZ',    2);

enuminfo.bladerf_trigger_role=struct('BLADERF_TRIGGER_ROLE_INVALID',   -1, ...
                                     'BLADERF_TRIGGER_ROLE_DISABLED',   0, ...
                                     'BLADERF_TRIGGER_ROLE_MASTER',     1, ...
                                     'BLADERF_TRIGGER_ROLE_SLAVE',      2);

enuminfo.bladerf_correction=struct('BLADERF_CORR_LMS_DCOFF_I',  0, ...
                                   'BLADERF_CORR_DCOFF_I',      0, ...
                                   'BLADERF_CORR_LMS_DCOFF_Q',  1, ...
                                   'BLADERF_CORR_DCOFF_Q',      1, ...
                                   'BLADERF_CORR_FPGA_PHASE',   2, ...
                                   'BLADERF_CORR_PHASE',        2, ...
                                   'BLADERF_CORR_FPGA_GAIN',    3, ...
                                   'BLADERF_CORR_GAIN',         3);

enuminfo.bladerf_xb200_path=struct('BLADERF_XB200_BYPASS',  0, ...
                                   'BLADERF_XB200_MIX',     1);

enuminfo.bladerf_trigger_signal=struct('BLADERF_TRIGGER_INVALID',  -1,   ...
                                       'BLADERF_TRIGGER_J71_4',     0,   ...
                                       'BLADERF_TRIGGER_USER_0',    128, ...
                                       'BLADERF_TRIGGER_USER_1',    129, ...
                                       'BLADERF_TRIGGER_USER_2',    130, ...
                                       'BLADERF_TRIGGER_USER_3',    131, ...
                                       'BLADERF_TRIGGER_USER_4',    132, ...
                                       'BLADERF_TRIGGER_USER_5',    133, ...
                                       'BLADERF_TRIGGER_USER_6',    134, ...
                                       'BLADERF_TRIGGER_USER_7',    135);

enuminfo.bladerf_lna_gain=struct('BLADERF_LNA_GAIN_UNKNOWN',    0, ...
                                 'BLADERF_LNA_GAIN_BYPASS',     1, ...
                                 'BLADERF_LNA_GAIN_MID',        2, ...
                                 'BLADERF_LNA_GAIN_MAX',        3);

enuminfo.bladerf_power_sources=struct('BLADERF_UNKNOWN',     0, ...
                                      'BLADERF_PS_DC',       1, ...
                                      'BLADERF_PS_USB_VBUS', 2);

enuminfo.bladerf_direction=struct('BLADERF_RX',        0, ...
                                  'BLADERF_MODULE_RX', 0, ...
                                  'BLADERF_TX',        1, ...
                                  'BLADERF_MODULE_TX', 1);

enuminfo.bladerf_channel_layout=struct('BLADERF_RX_X1',  0, ...
                                       'BLADERF_TX_X1',  1, ...
                                       'BLADERF_RX_X2',  2, ...
                                       'BLADERF_TX_X2',  3);

enuminfo.bladerf_pmic_register=struct('BLADERF_PMIC_CONFIGURATION',  0, ...
                                      'BLADERF_PMIC_VOLTAGE_SHUNT',  1, ...
                                      'BLADERF_PMIC_VOLTAGE_BUS',    2, ...
                                      'BLADERF_PMIC_POWER',          3, ...
                                      'BLADERF_PMIC_CURRENT',        4, ...
                                      'BLADERF_PMIC_CALIBRATION',    5);

enuminfo.bladerf_clock_select=struct('CLOCK_SELECT_ONBOARD',    0, ...
                                     'CLOCK_SELECT_EXTERNAL',  1);


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Output
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

methodinfo=fcns;
