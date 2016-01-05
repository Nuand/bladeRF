%
% Copyright (c) 2015 Nuand LLC
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

function [methodinfo,structs,enuminfo,ThunkLibName]=libbladeRF_proto
%LIBBLADERF_PROTO Create structures to define interfaces found in libbladeRF.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Setup
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

ival={cell(1, 105)};
structs=[];
enuminfo=[];
fcnNum=1;
fcns=struct('name',ival,'calltype',ival,'LHS',ival,'RHS',ival,'alias',ival,'thunkname', ival);

arch = computer('arch');
switch arch
    case 'glnxa64'
        libname   = 'libbladeRF_thunk_glnxa64';
        libsuffix = '.so';
        pathsep   = ':';
        bool_arg  = 'bool';
        bool_ret  = '_Bool';
        u64_type  = 'ulong';

    case 'win64'
        libname   = 'libbladeRF_thunk_pcwin64';
        libsuffix = '.dll';
        bool_arg  = 'uint8';
        bool_ret  = 'uint8';
        u64_type  = 'uint64';
        pathsep   = ';';

    case 'imac64'
        % Additionaly type changes for this may be required for OSX support
        libsuffix = '.dylib';
        pathsep   = ':';

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

% int bladerf_open_with_devinfo ( struct bladerf ** device , struct bladerf_devinfo * devinfo );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_open_with_devinfo'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtrPtr', 'bladerf_devinfoPtr'};fcnNum=fcnNum+1;

% int bladerf_open ( struct bladerf ** device , const char * device_identifier );
fcns.thunkname{fcnNum}='int32voidPtrcstringThunk';fcns.name{fcnNum}='bladerf_open'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtrPtr', 'cstring'};fcnNum=fcnNum+1;

% void bladerf_close ( struct bladerf * device );
fcns.thunkname{fcnNum}='voidvoidPtrThunk';fcns.name{fcnNum}='bladerf_close'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={'bladerfPtr'};fcnNum=fcnNum+1;

% void bladerf_set_usb_reset_on_open ( _Bool enabled );
fcns.thunkname{fcnNum}=['void' bool_ret 'Thunk'];fcns.name{fcnNum}='bladerf_set_usb_reset_on_open'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={bool_arg};fcnNum=fcnNum+1;

% void bladerf_init_devinfo ( struct bladerf_devinfo * info );
fcns.thunkname{fcnNum}='voidvoidPtrThunk';fcns.name{fcnNum}='bladerf_init_devinfo'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={'bladerf_devinfoPtr'};fcnNum=fcnNum+1;

% int bladerf_get_devinfo ( struct bladerf * dev , struct bladerf_devinfo * info );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_devinfo'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_devinfoPtr'};fcnNum=fcnNum+1;

% int bladerf_get_devinfo_from_str ( const char * devstr , struct bladerf_devinfo * info );
fcns.thunkname{fcnNum}='int32cstringvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_devinfo_from_str'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'cstring', 'bladerf_devinfoPtr'};fcnNum=fcnNum+1;

% _Bool bladerf_devinfo_matches ( const struct bladerf_devinfo * a , const struct bladerf_devinfo * b );
fcns.thunkname{fcnNum}=[bool_ret 'voidPtrvoidPtrThunk'];fcns.name{fcnNum}='bladerf_devinfo_matches'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=bool_arg; fcns.RHS{fcnNum}={'bladerf_devinfoPtr', 'bladerf_devinfoPtr'};fcnNum=fcnNum+1;

% _Bool bladerf_devstr_matches ( const char * dev_str , struct bladerf_devinfo * info );
fcns.thunkname{fcnNum}=[bool_ret 'cstringvoidPtrThunk'];fcns.name{fcnNum}='bladerf_devstr_matches'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=bool_arg; fcns.RHS{fcnNum}={'cstring', 'bladerf_devinfoPtr'};fcnNum=fcnNum+1;

% const char * bladerf_backend_str ( bladerf_backend backend );
fcns.thunkname{fcnNum}='cstringbladerf_backendThunk';fcns.name{fcnNum}='bladerf_backend_str'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='cstring'; fcns.RHS{fcnNum}={'bladerf_backend'};fcnNum=fcnNum+1;

% int bladerf_enable_module ( struct bladerf * dev , bladerf_module m , _Bool enable );
fcns.thunkname{fcnNum}=['int32voidPtrbladerf_module' bool_ret 'Thunk'];fcns.name{fcnNum}='bladerf_enable_module'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', bool_arg};fcnNum=fcnNum+1;

% int bladerf_set_loopback ( struct bladerf * dev , bladerf_loopback l );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_loopbackThunk';fcns.name{fcnNum}='bladerf_set_loopback'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_loopback'};fcnNum=fcnNum+1;

% int bladerf_get_loopback ( struct bladerf * dev , bladerf_loopback * l );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_loopback'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_loopbackPtr'};fcnNum=fcnNum+1;

% int bladerf_set_rx_mux ( struct bladerf * dev , bladerf_rx_mux mux );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_rx_muxThunk';fcns.name{fcnNum}='bladerf_set_rx_mux'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_rx_mux'};fcnNum=fcnNum+1;

% int bladerf_get_rx_mux ( struct bladerf * dev , bladerf_rx_mux * mux );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_rx_mux'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_rx_muxPtr'};fcnNum=fcnNum+1;

% int bladerf_set_sample_rate ( struct bladerf * dev , bladerf_module module , unsigned int rate , unsigned int * actual );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_moduleuint32voidPtrThunk';fcns.name{fcnNum}='bladerf_set_sample_rate'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'uint32', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_set_rational_sample_rate ( struct bladerf * dev , bladerf_module module , struct bladerf_rational_rate * rate , struct bladerf_rational_rate * actual );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulevoidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_set_rational_sample_rate'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'bladerf_rational_ratePtr', 'bladerf_rational_ratePtr'};fcnNum=fcnNum+1;

% int bladerf_set_sampling ( struct bladerf * dev , bladerf_sampling sampling );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_samplingThunk';fcns.name{fcnNum}='bladerf_set_sampling'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_sampling'};fcnNum=fcnNum+1;

% int bladerf_get_sampling ( struct bladerf * dev , bladerf_sampling * sampling );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_sampling'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_samplingPtr'};fcnNum=fcnNum+1;

% int bladerf_get_sample_rate ( struct bladerf * dev , bladerf_module module , unsigned int * rate );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulevoidPtrThunk';fcns.name{fcnNum}='bladerf_get_sample_rate'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_get_rational_sample_rate ( struct bladerf * dev , bladerf_module module , struct bladerf_rational_rate * rate );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulevoidPtrThunk';fcns.name{fcnNum}='bladerf_get_rational_sample_rate'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'bladerf_rational_ratePtr'};fcnNum=fcnNum+1;

% int bladerf_set_rational_smb_frequency ( struct bladerf * dev , struct bladerf_rational_rate * rate , struct bladerf_rational_rate * actual );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_set_rational_smb_frequency'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_rational_ratePtr', 'bladerf_rational_ratePtr'};fcnNum=fcnNum+1;

% int bladerf_set_smb_frequency ( struct bladerf * dev , uint32_t rate , uint32_t * actual );
fcns.thunkname{fcnNum}='int32voidPtruint32voidPtrThunk';fcns.name{fcnNum}='bladerf_set_smb_frequency'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_get_rational_smb_frequency ( struct bladerf * dev , struct bladerf_rational_rate * rate );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_rational_smb_frequency'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_rational_ratePtr'};fcnNum=fcnNum+1;

% int bladerf_get_smb_frequency ( struct bladerf * dev , unsigned int * rate );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_smb_frequency'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_set_correction ( struct bladerf * dev , bladerf_module module , bladerf_correction corr , int16_t value );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulebladerf_correctionint16Thunk';fcns.name{fcnNum}='bladerf_set_correction'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'bladerf_correction', 'int16'};fcnNum=fcnNum+1;

% int bladerf_get_correction ( struct bladerf * dev , bladerf_module module , bladerf_correction corr , int16_t * value );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulebladerf_correctionvoidPtrThunk';fcns.name{fcnNum}='bladerf_get_correction'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'bladerf_correction', 'int16Ptr'};fcnNum=fcnNum+1;

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

% int bladerf_set_gain ( struct bladerf * dev , bladerf_module mod , int gain );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_moduleint32Thunk';fcns.name{fcnNum}='bladerf_set_gain'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'int32'};fcnNum=fcnNum+1;

% int bladerf_set_bandwidth ( struct bladerf * dev , bladerf_module module , unsigned int bandwidth , unsigned int * actual );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_moduleuint32voidPtrThunk';fcns.name{fcnNum}='bladerf_set_bandwidth'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'uint32', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_get_bandwidth ( struct bladerf * dev , bladerf_module module , unsigned int * bandwidth );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulevoidPtrThunk';fcns.name{fcnNum}='bladerf_get_bandwidth'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_set_lpf_mode ( struct bladerf * dev , bladerf_module module , bladerf_lpf_mode mode );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulebladerf_lpf_modeThunk';fcns.name{fcnNum}='bladerf_set_lpf_mode'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'bladerf_lpf_mode'};fcnNum=fcnNum+1;

% int bladerf_get_lpf_mode ( struct bladerf * dev , bladerf_module module , bladerf_lpf_mode * mode );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulevoidPtrThunk';fcns.name{fcnNum}='bladerf_get_lpf_mode'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'bladerf_lpf_modePtr'};fcnNum=fcnNum+1;

% int bladerf_select_band ( struct bladerf * dev , bladerf_module module , unsigned int frequency );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_moduleuint32Thunk';fcns.name{fcnNum}='bladerf_select_band'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_set_frequency ( struct bladerf * dev , bladerf_module module , unsigned int frequency );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_moduleuint32Thunk';fcns.name{fcnNum}='bladerf_set_frequency'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_schedule_retune ( struct bladerf * dev , bladerf_module module , uint64_t timestamp , unsigned int frequency , struct bladerf_quick_tune * quick_tune );
fcns.thunkname{fcnNum}=['int32voidPtrbladerf_module' u64_type 'uint32voidPtrThunk'];fcns.name{fcnNum}='bladerf_schedule_retune'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', u64_type, 'uint32', 'bladerf_quick_tunePtr'};fcnNum=fcnNum+1;

% int bladerf_cancel_scheduled_retunes ( struct bladerf * dev , bladerf_module module );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_moduleThunk';fcns.name{fcnNum}='bladerf_cancel_scheduled_retunes'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module'};fcnNum=fcnNum+1;

% int bladerf_get_frequency ( struct bladerf * dev , bladerf_module module , unsigned int * frequency );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulevoidPtrThunk';fcns.name{fcnNum}='bladerf_get_frequency'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_get_quick_tune ( struct bladerf * dev , bladerf_module module , struct bladerf_quick_tune * quick_tune );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulevoidPtrThunk';fcns.name{fcnNum}='bladerf_get_quick_tune'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'bladerf_quick_tunePtr'};fcnNum=fcnNum+1;

% int bladerf_set_tuning_mode ( struct bladerf * dev , bladerf_tuning_mode mode );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_tuning_modeThunk';fcns.name{fcnNum}='bladerf_set_tuning_mode'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_tuning_mode'};fcnNum=fcnNum+1;

% int bladerf_expansion_attach ( struct bladerf * dev , bladerf_xb xb );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_xbThunk';fcns.name{fcnNum}='bladerf_expansion_attach'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_xb'};fcnNum=fcnNum+1;

% int bladerf_expansion_get_attached ( struct bladerf * dev , bladerf_xb * xb );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_expansion_get_attached'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_xbPtr'};fcnNum=fcnNum+1;

% int bladerf_xb200_set_filterbank ( struct bladerf * dev , bladerf_module mod , bladerf_xb200_filter filter );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulebladerf_xb200_filterThunk';fcns.name{fcnNum}='bladerf_xb200_set_filterbank'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'bladerf_xb200_filter'};fcnNum=fcnNum+1;

% int bladerf_xb200_get_filterbank ( struct bladerf * dev , bladerf_module module , bladerf_xb200_filter * filter );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulevoidPtrThunk';fcns.name{fcnNum}='bladerf_xb200_get_filterbank'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'bladerf_xb200_filterPtr'};fcnNum=fcnNum+1;

% int bladerf_xb200_set_path ( struct bladerf * dev , bladerf_module module , bladerf_xb200_path path );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulebladerf_xb200_pathThunk';fcns.name{fcnNum}='bladerf_xb200_set_path'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'bladerf_xb200_path'};fcnNum=fcnNum+1;

% int bladerf_xb200_get_path ( struct bladerf * dev , bladerf_module module , bladerf_xb200_path * path );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulevoidPtrThunk';fcns.name{fcnNum}='bladerf_xb200_get_path'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'bladerf_xb200_pathPtr'};fcnNum=fcnNum+1;

% int bladerf_set_stream_timeout ( struct bladerf * dev , bladerf_module module , unsigned int timeout );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_moduleuint32Thunk';fcns.name{fcnNum}='bladerf_set_stream_timeout'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_get_stream_timeout ( struct bladerf * dev , bladerf_module module , unsigned int * timeout );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulevoidPtrThunk';fcns.name{fcnNum}='bladerf_get_stream_timeout'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'uint32Ptr'};fcnNum=fcnNum+1;

% int bladerf_sync_config ( struct bladerf * dev , bladerf_module module , bladerf_format format , unsigned int num_buffers , unsigned int buffer_size , unsigned int num_transfers , unsigned int stream_timeout );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulebladerf_formatuint32uint32uint32uint32Thunk';fcns.name{fcnNum}='bladerf_sync_config'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', 'bladerf_format', 'uint32', 'uint32', 'uint32', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_sync_tx ( struct bladerf * dev , void * samples , unsigned int num_samples , struct bladerf_metadata * metadata , unsigned int timeout_ms );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtruint32voidPtruint32Thunk';fcns.name{fcnNum}='bladerf_sync_tx'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'voidPtr', 'uint32', 'bladerf_metadataPtr', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_sync_rx ( struct bladerf * dev , void * samples , unsigned int num_samples , struct bladerf_metadata * metadata , unsigned int timeout_ms );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtruint32voidPtruint32Thunk';fcns.name{fcnNum}='bladerf_sync_rx'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'voidPtr', 'uint32', 'bladerf_metadataPtr', 'uint32'};fcnNum=fcnNum+1;

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

% const char * bladerf_strerror ( int error );
fcns.thunkname{fcnNum}='cstringint32Thunk';fcns.name{fcnNum}='bladerf_strerror'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='cstring'; fcns.RHS{fcnNum}={'int32'};fcnNum=fcnNum+1;

% void bladerf_version ( struct bladerf_version * version );
fcns.thunkname{fcnNum}='voidvoidPtrThunk';fcns.name{fcnNum}='bladerf_version'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={'bladerf_versionPtr'};fcnNum=fcnNum+1;

% void bladerf_log_set_verbosity ( bladerf_log_level level );
fcns.thunkname{fcnNum}='voidbladerf_log_levelThunk';fcns.name{fcnNum}='bladerf_log_set_verbosity'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={'bladerf_log_level'};fcnNum=fcnNum+1;

% struct bladerf_image * bladerf_alloc_image ( bladerf_image_type type , uint32_t address , uint32_t length );
fcns.thunkname{fcnNum}='voidPtrbladerf_image_typeuint32uint32Thunk';fcns.name{fcnNum}='bladerf_alloc_image'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='bladerf_imagePtr'; fcns.RHS{fcnNum}={'bladerf_image_type', 'uint32', 'uint32'};fcnNum=fcnNum+1;

% struct bladerf_image * bladerf_alloc_cal_image ( bladerf_fpga_size fpga_size , uint16_t vctcxo_trim );
fcns.thunkname{fcnNum}='voidPtrbladerf_fpga_sizeuint16Thunk';fcns.name{fcnNum}='bladerf_alloc_cal_image'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='bladerf_imagePtr'; fcns.RHS{fcnNum}={'bladerf_fpga_size', 'uint16'};fcnNum=fcnNum+1;

% void bladerf_free_image ( struct bladerf_image * image );
fcns.thunkname{fcnNum}='voidvoidPtrThunk';fcns.name{fcnNum}='bladerf_free_image'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}=[]; fcns.RHS{fcnNum}={'bladerf_imagePtr'};fcnNum=fcnNum+1;

% int bladerf_image_write ( struct bladerf_image * image , const char * file );
fcns.thunkname{fcnNum}='int32voidPtrcstringThunk';fcns.name{fcnNum}='bladerf_image_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerf_imagePtr', 'cstring'};fcnNum=fcnNum+1;

% int bladerf_image_read ( struct bladerf_image * image , const char * file );
fcns.thunkname{fcnNum}='int32voidPtrcstringThunk';fcns.name{fcnNum}='bladerf_image_read'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerf_imagePtr', 'cstring'};fcnNum=fcnNum+1;

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

% int bladerf_get_timestamp ( struct bladerf * dev , bladerf_module mod , uint64_t * value );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_modulevoidPtrThunk';fcns.name{fcnNum}='bladerf_get_timestamp'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_module', [u64_type 'Ptr']};fcnNum=fcnNum+1;

% int bladerf_dac_write ( struct bladerf * dev , uint16_t val );
fcns.thunkname{fcnNum}='int32voidPtruint16Thunk';fcns.name{fcnNum}='bladerf_dac_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint16'};fcnNum=fcnNum+1;

% int bladerf_dac_read ( struct bladerf * dev , uint16_t * val );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtrThunk';fcns.name{fcnNum}='bladerf_dac_read'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint16Ptr'};fcnNum=fcnNum+1;

% int bladerf_xb_spi_write ( struct bladerf * dev , uint32_t val );
fcns.thunkname{fcnNum}='int32voidPtruint32Thunk';fcns.name{fcnNum}='bladerf_xb_spi_write'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_calibrate_dc ( struct bladerf * dev , bladerf_cal_module module );
fcns.thunkname{fcnNum}='int32voidPtrbladerf_cal_moduleThunk';fcns.name{fcnNum}='bladerf_calibrate_dc'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'bladerf_cal_module'};fcnNum=fcnNum+1;

% int bladerf_erase_flash ( struct bladerf * dev , uint32_t erase_block , uint32_t count );
fcns.thunkname{fcnNum}='int32voidPtruint32uint32Thunk';fcns.name{fcnNum}='bladerf_erase_flash'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint32', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_read_flash ( struct bladerf * dev , uint8_t * buf , uint32_t page , uint32_t count );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtruint32uint32Thunk';fcns.name{fcnNum}='bladerf_read_flash'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint8Ptr', 'uint32', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_write_flash ( struct bladerf * dev , const uint8_t * buf , uint32_t page , uint32_t count );
fcns.thunkname{fcnNum}='int32voidPtrvoidPtruint32uint32Thunk';fcns.name{fcnNum}='bladerf_write_flash'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerfPtr', 'uint8Ptr', 'uint32', 'uint32'};fcnNum=fcnNum+1;

% int bladerf_get_bootloader_list ( struct bladerf_devinfo ** list );
fcns.thunkname{fcnNum}='int32voidPtrThunk';fcns.name{fcnNum}='bladerf_get_bootloader_list'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'bladerf_devinfoPtrPtr'};fcnNum=fcnNum+1;

% int bladerf_load_fw_from_bootloader ( const char * device_identifier , bladerf_backend backend , uint8_t bus , uint8_t addr , const char * file );
fcns.thunkname{fcnNum}='int32cstringbladerf_backenduint8uint8cstringThunk';fcns.name{fcnNum}='bladerf_load_fw_from_bootloader'; fcns.calltype{fcnNum}='Thunk'; fcns.LHS{fcnNum}='int32'; fcns.RHS{fcnNum}={'cstring', 'bladerf_backend', 'uint8', 'uint8', 'cstring'};fcnNum=fcnNum+1;


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Structures
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

structs.bladerf.members=struct('');

structs.bladerf_devinfo.members=struct('backend',  'bladerf_backend', ...
                                       'serial',   'int8#33', ...
                                       'usb_bus',  'uint8', ...
                                       'usb_addr', 'uint8', ...
                                       'instance', 'uint32');

structs.bladerf_rational_rate.members=struct('integer', u64_type, ...
                                             'num',     u64_type, ...
                                             'den',     u64_type);

structs.bladerf_quick_tune.members=struct('freqsel', 'uint8', ...
                                          'vcocap',  'uint8', ...
                                          'nint',    'uint16', ...
                                          'nfrac',   'uint32', ...
                                          'flags', 'uint8');

structs.bladerf_metadata.members=struct('timestamp',     u64_type, ...
                                        'flags',        'uint32', ...
                                        'status',       'uint32', ...
                                        'actual_count', 'uint32', ...
                                        'reserved',     'uint8#32');

structs.bladerf_version.members=struct('major',    'uint16', ...
                                       'minor',    'uint16', ...
                                       'patch',    'uint16', ...
                                       'describe', 'cstring');

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

% Workaround for MATLAB <= 2015b (64-bit) not identifying the size of the devinfo structure correctly
structs.bladerf_devinfo.packing=4;


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Enumerations
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

enuminfo.bladerf_tuning_mode=struct('BLADERF_TUNING_MODE_INVALID', -1, ...
                                    'BLADERF_TUNING_MODE_HOST',     0, ...
                                    'BLADERF_TUNING_MODE_FPGA',     1);

enuminfo.bladerf_format=struct('BLADERF_FORMAT_SC16_Q11',       0, ...
                               'BLADERF_FORMAT_SC16_Q11_META',  1);

enuminfo.bladerf_lpf_mode=struct('BLADERF_LPF_NORMAL',      0, ...
                                 'BLADERF_LPF_BYPASSED',    1, ...
                                 'BLADERF_LPF_DISABLED',    2);

enuminfo.bladerf_xb200_path=struct('BLADERF_XB200_BYPASS',  0, ...
                                   'BLADERF_XB200_MIX',     1);

enuminfo.bladerf_correction=struct('BLADERF_CORR_LMS_DCOFF_I',  0, ...
                                   'BLADERF_CORR_LMS_DCOFF_Q',  1, ...
                                   'BLADERF_CORR_FPGA_PHASE',   2, ...
                                   'BLADERF_CORR_FPGA_GAIN',    3);

enuminfo.bladerf_cal_module=struct('BLADERF_DC_CAL_LPF_TUNING', 0, ...
                                   'BLADERF_DC_CAL_TX_LPF',     1, ...
                                   'BLADERF_DC_CAL_RX_LPF',     2, ...
                                   'BLADERF_DC_CAL_RXVGA2',     3);

enuminfo.bladerf_log_level=struct('BLADERF_LOG_LEVEL_VERBOSE',  0, ...
                                  'BLADERF_LOG_LEVEL_DEBUG',    1, ...
                                  'BLADERF_LOG_LEVEL_INFO',     2, ...
                                  'BLADERF_LOG_LEVEL_WARNING',  3, ...
                                  'BLADERF_LOG_LEVEL_ERROR',    4, ...
                                  'BLADERF_LOG_LEVEL_CRITICAL', 5, ...
                                  'BLADERF_LOG_LEVEL_SILENT',   6);

enuminfo.bladerf_loopback=struct('BLADERF_LB_FIRMWARE',         1, ...
                                 'BLADERF_LB_BB_TXLPF_RXVGA2',  2, ...
                                 'BLADERF_LB_BB_TXVGA1_RXVGA2', 3, ...
                                 'BLADERF_LB_BB_TXLPF_RXLPF',   4, ...
                                 'BLADERF_LB_BB_TXVGA1_RXLPF',  5, ...
                                 'BLADERF_LB_RF_LNA1',          6, ...
                                 'BLADERF_LB_RF_LNA2',          7, ...
                                 'BLADERF_LB_RF_LNA3',          8, ...
                                 'BLADERF_LB_NONE',             9);

enuminfo.bladerf_lna_gain=struct('BLADERF_LNA_GAIN_UNKNOWN',    0, ...
                                 'BLADERF_LNA_GAIN_BYPASS',     1, ...
                                 'BLADERF_LNA_GAIN_MID',        2, ...
                                 'BLADERF_LNA_GAIN_MAX',        3);

enuminfo.bladerf_sampling=struct('BLADERF_SAMPLING_UNKNOWN',    0, ...
                                 'BLADERF_SAMPLING_INTERNAL',   1, ...
                                 'BLADERF_SAMPLING_EXTERNAL',   2);

enuminfo.bladerf_module=struct('BLADERF_MODULE_RX', 0, ...
                               'BLADERF_MODULE_TX', 1);

enuminfo.bladerf_dev_speed=struct('BLADERF_DEVICE_SPEED_UNKNOWN',   0, ...
                                  'BLADERF_DEVICE_SPEED_HIGH',      1, ...
                                  'BLADERF_DEVICE_SPEED_SUPER',     2);

enuminfo.bladerf_xb=struct('BLADERF_XB_NONE',   0, ...
                           'BLADERF_XB_100',    1, ...
                           'BLADERF_XB_200',    2);

enuminfo.bladerf_backend=struct('BLADERF_BACKEND_ANY',      0, ...
                                'BLADERF_BACKEND_LINUX',    1, ...
                                'BLADERF_BACKEND_LIBUSB',   2, ...
                                'BLADERF_BACKEND_CYPRESS',  3, ...
                                'BLADERF_BACKEND_DUMMY',    100);

enuminfo.bladerf_fpga_size=struct('BLADERF_FPGA_UNKNOWN',   0,  ...
                                  'BLADERF_FPGA_40KLE',     40, ...
                                  'BLADERF_FPGA_115KLE',    115);

enuminfo.bladerf_xb200_filter=struct('BLADERF_XB200_50M',       0, ...
                                     'BLADERF_XB200_144M',      1, ...
                                     'BLADERF_XB200_222M',      2, ...
                                     'BLADERF_XB200_CUSTOM',    3, ...
                                     'BLADERF_XB200_AUTO_1DB',  4, ...
                                     'BLADERF_XB200_AUTO_3DB',  5);

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

enuminfo.bladerf_rx_mux=struct('BLADERF_RX_MUX_INVALID',            -1, ...
                               'BLADERF_RX_MUX_BASEBAND_LMS',        0, ...
                               'BLADERF_RX_MUX_12BIT_COUNTER',       1, ...
                               'BLADERF_RX_MUX_32BIT_COUNTER',       2, ...
                               'BLADERF_RX_MUX_DIGITAL_LOOPBACK',    4);
                               % Value 3 is reserved for future use

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Output
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

methodinfo=fcns;
