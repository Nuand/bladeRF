% A simple bladeRF demo GUI that receives and plots samples
%
% Once the GUI starts, select a device from the dropdown of available
% devices, and then click "start" to begin streaming samples.
%
% The various GUI widgets may be used to change the plot mode, frequency,
% gains, correction values, sample rate, and LPF bandwidth.
%
% Note that stream settings may only be changed whe the device is not
% actively streaming.

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

function varargout = bladeRF_fft(varargin)
    gui_Singleton = 1;
    gui_State = struct('gui_Name',       mfilename, ...
                       'gui_Singleton',  gui_Singleton, ...
                       'gui_OpeningFcn', @bladeRF_fft_OpeningFcn, ...
                       'gui_OutputFcn',  @bladeRF_fft_OutputFcn, ...
                       'gui_LayoutFcn',  [] , ...
                       'gui_Callback',   []);

    if nargin && ischar(varargin{1})
        gui_State.gui_Callback = str2func(varargin{1});
    end

    if nargout
        [varargout{1:nargout}] = gui_mainfcn(gui_State, varargin{:});
    else
        gui_mainfcn(gui_State, varargin{:});
    end
end

function set_bandwidth_selection(bandwidth_widget, value)
    strings = bandwidth_widget.String;

    for n = 1:length(strings)
        if value == (str2num(strings{n}) * 1e6)
            bandwidth_widget.Value = n;
        end
    end
end

function set_lnagain_selection(lnagain_widget, value)
    strings = lnagain_widget.String;
    for n = 1:length(strings)
        if strcmpi(strings{n}, value)
            lnagain_widget.Value = n;
        end
    end
end

function update_plot_selection(hObject, handles)
    plots = get_plots(hObject);

    id = handles.displaytype.Value;
    if id < 1 || id > length(plots)
        error('Bug: Got invalid display type ID');
    end

    for n = 1:length(plots)
        if n == id
            for l = 1:length(plots{n}.lines)
                plots{n}.lines(l).Visible = 'on';
                %fprintf('plot %s ch %d = %s\n', plots{n}.name, l, plots{n}.lines(l).Visible);
            end
        else
           for l = 1:length(plots{n}.lines)
                plots{n}.lines(l).Visible = 'off';
                %fprintf('plot %s ch %d = %s\n', plots{n}.name, l, plots{n}.lines(l).Visible);
           end
        end
    end

    num_samples = get_num_samples(hObject);

    % Reset data so we don't see "random" junk when switching displays
    switch plots{id}.name
        case { 'FFT (dB)', 'FFT (linear)' }
            x = linspace(double(plots{id}.xmin), double(plots{id}.xmax), num_samples);
            plots{id}.lines(1).XData = x;
            plots{id}.lines(1).YData = zeros(num_samples, 1) - plots{id}.ymin;

        case 'Time (2-Channel)'
            x = linspace(double(plots{id}.xmin), double(plots{id}.xmax), num_samples);

            plots{id}.lines(1).XData = x;
            plots{id}.lines(1).YData = zeros(num_samples, 1);

            plots{id}.lines(2).XData = x;
            plots{id}.lines(2).YData = zeros(num_samples, 1);

        case 'Time (XY)'
            plots{id}.lines(1).XData = zeros(num_samples, 1);
            plots{id}.lines(1).YData = zeros(num_samples, 1);
    end

    % Update the axes limits for this plot
    handles.axes1.XLim = [plots{id}.xmin plots{id}.xmax];
    handles.axes1.YLim = [plots{id}.ymin plots{id}.ymax];

    % Update the axis labels
    xlabel(handles.axes1, plots{id}.xlabel);
    ylabel(handles.axes1, plots{id}.ylabel);
end

% Get the handle to the GUI's root object
function [root] = get_root_object(hObject)
    if strcmp(hObject.Type, 'root')
        root = hObject;
    else
        root = get_root_object(hObject.Parent);
    end
end

% Get the state of the 'Print Overruns' flag
function [print_overruns] = get_print_overruns(hObject)
    root = get_root_object(hObject);
    print_overruns = getappdata(root, 'print_overruns');
    if isempty(print_overruns)
        error('Failed to access app data: print_overruns');
    end
end

% Set the state of the 'Print Overruns' flag
function set_print_overruns(hObject, value)
    root = get_root_object(hObject);
    setappdata(root, 'print_overruns', value);
end

% Get the number of samples that are retrieved from the device per RX
function [num_samples] = get_num_samples(hObject)
    root = get_root_object(hObject);
    num_samples = getappdata(root, 'num_samples');
    if isempty(num_samples)
        error('Failed to access app data: num_samples');
    end
end

% Get the array of plots
function [plots] = get_plots(hObject)
    root = get_root_object(hObject);
    plots = getappdata(root, 'plots');
    if isempty(plots)
        error('Failed to access app data: plot');
    end
end

% Apply updates to plot configuration
function set_plots(hObject, plots)
   root = get_root_object(hObject);
   setappdata(root, 'plots', plots);
end

% Get the frame rate configuration
function [fps] = get_framerate(hObject)
   root = get_root_object(hObject);
   fps = getappdata(root, 'framerate');
end

% Set the fraem rate configuration
function set_framerate(hObject, fps)
   root = get_root_object(hObject);
   setappdata(root, 'framerate', fps);
end

function update_plot_axes(hObject, handles)
    plots = get_plots(hObject);

    Fc = handles.bladerf.rx.frequency;
    Fs = handles.bladerf.rx.samplerate;
    num_samples = get_num_samples(hObject);

    % Update the axes limits of all plots
    for id = 1:length(plots)
        switch plots{id}.name
            case 'FFT (dB)'
                plots{id}.xmin = (Fc - Fs/2);
                plots{id}.xmax = (Fc + Fs/2);
                plots{id}.ymin = -110;
                plots{id}.ymax = 0;

                % Ensure the X values are updated, as these are not updated every read
                if id == handles.displaytype.Value
                    x = linspace(double(plots{id}.xmin), double(plots{id}.xmax), num_samples);
                    plots{id}.lines(1).XData = x;
                end

            case 'FFT (linear)'
                plots{id}.xmin = (Fc - Fs/2);
                plots{id}.xmax = (Fc + Fs/2);
                plots{id}.ymin = 0;
                plots{id}.ymax = 1;

                % Ensure the X values are updated, as these are not updated every read
                if id == handles.displaytype.Value
                    x = linspace(double(plots{id}.xmin), double(plots{id}.xmax), num_samples);
                    plots{id}.lines(1).XData = x;
                end

            case 'Time (2-Channel)'
                plots{id}.xmin = 0;
                plots{id}.xmax = (num_samples - 1) / Fs;
                plots{id}.ymin = -1.2;
                plots{id}.ymax =  1.2;

            case 'Time (XY)'
                plots{id}.xmin = -1.2;
                plots{id}.xmax =  1.2;
                plots{id}.ymin = -1.2;
                plots{id}.ymax =  1.2;

            otherwise
                error('Invalid plot type encountered');
        end

        % Update the current plot axes
        if id == handles.displaytype.Value
            handles.axes1.XLim = [plots{id}.xmin plots{id}.xmax];
            handles.axes1.YLim = [plots{id}.ymin plots{id}.ymax];
        end
    end

    set_plots(hObject, plots);
end

function [plot_info] = init_plot_type(hObject, handles, type)

    plot_info.name = type;

    blue = [0 0 1];
    red  = [1 0 0];

    num_samples = get_num_samples(hObject);

    x = zeros(num_samples, 1);
    y = zeros(num_samples, 1);

    switch type
        case { 'FFT (dB)', 'FFT (linear)' }
            plot_info.xlabel = 'Frequency (MHz)';
            plot_info.ylabel = 'Power (dB)';
            plot_info.lines(1) = line(x, y);
            plot_info.lines(1).Color = blue;
            plot_info.lines(1).Marker = 'none';
            plot_info.lines(1).LineStyle = '-';

        case 'Time (2-Channel)'
            plot_info.xlabel = 'Time (s)';
            plot_info.ylabel = 'Sample Values (I: Blue, Q: Red)';
            plot_info.lines(1) = line(x, y);
            plot_info.lines(1).Color = blue;
            plot_info.lines(1).Marker = 'none';
            plot_info.lines(1).LineStyle = '-';

            plot_info.lines(2) = line(x, y);
            plot_info.lines(2).Color = red;
            plot_info.lines(2).Marker = 'none';
            plot_info.lines(2).LineStyle = '-';

        case 'Time (XY)'
            plot_info.xlabel = 'I Value';
            plot_info.ylabel = 'Q Value';
            plot_info.lines(1) = line(x, y);
            plot_info.lines(1).Color = blue;
            plot_info.lines(1).Marker = '.';
            plot_info.lines(1).LineStyle = 'none';
            plot_info.lines(1).Visible = 'off';

        otherwise
            error('Invalid plot type encountered');
    end
end

% Update GUI fields with parameter values from device
function read_device_parameters(hObject, handles)
    handles.vga1.String = num2str(handles.bladerf.rx.vga1);
    handles.vga2.String = num2str(handles.bladerf.rx.vga2);
    set_lnagain_selection(handles.lnagain, handles.bladerf.rx.lna);

    handles.corr_dc_i.String  = num2str(handles.bladerf.rx.corrections.dc_i);
    handles.corr_dc_q.String  = num2str(handles.bladerf.rx.corrections.dc_q);
    handles.corr_gain.String  = num2str(handles.bladerf.rx.corrections.gain);
    handles.corr_phase.String = num2str(handles.bladerf.rx.corrections.phase);

    handles.frequency.String  = num2str(handles.bladerf.rx.frequency);
    handles.samplerate.String = num2str(handles.bladerf.rx.samplerate);
    set_bandwidth_selection(handles.bandwidth, handles.bladerf.rx.bandwidth);

    handles.num_buffers.String = num2str(handles.bladerf.rx.config.num_buffers);
    handles.buffer_size.String = num2str(handles.bladerf.rx.config.buffer_size);
    handles.num_transfers.String = num2str(handles.bladerf.rx.config.num_transfers);

    guidata(hObject, handles);
end

function bladeRF_fft_OpeningFcn(hObject, ~, handles, varargin)
    % Choose default command line output for bladeRF_fft
    handles.output = hObject;

    % UIWAIT makes bladeRF_fft wait for user response (see UIRESUME)
    % uiwait(handles.figure1);
    handles.bladerf = bladeRF('*:instance=0');

    read_device_parameters(hObject, handles);

    handles.bladerf.rx.config.num_buffers = 64;
    handles.bladerf.rx.config.buffer_size = 16384;
    handles.bladerf.rx.config.timeout_ms = 5000;

    handles.print_overruns.Value = 0;
    setappdata(hObject.Parent, 'print_overruns', 0);

    setappdata(hObject.Parent, 'run', 0);

    % Number of samples we'll read from the device at each iteraion
    setappdata(hObject.Parent, 'num_samples', 4096);

    % Default frame rate
    setappdata(hObject.Parent, 'framerate', 15);
    handles.framerate.String = '15';

    %  Create plot information for each type of lot
    type_strs = handles.displaytype.String;

    plots = cell(1, length(type_strs));
    for n = 1:length(plots)
        plots{n} = init_plot_type(hObject.Parent, handles, type_strs{n});
    end

    setappdata(hObject.Parent, 'plots', plots);

    update_plot_axes(hObject, handles);
    update_plot_selection(hObject.Parent, handles);

    % Update handles structure
    guidata(hObject, handles);
end

function varargout = bladeRF_fft_OutputFcn(~, ~, handles)
    varargout{1} = handles.output;
end

function displaytype_Callback(hObject, ~, handles)
    update_plot_selection(hObject.Parent.Parent, handles);
end

function displaytype_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function actionbutton_Callback(hObject, ~, handles)
    action = hObject.String;
    switch action
        case 'Start'
            handles.buffer_size.Enable = 'off';
            handles.num_buffers.Enable = 'off';
            handles.num_transfers.Enable = 'off';
            handles.devicelist.Enable = 'off';
            handles.xb200_attached.Enable = 'off';

            handles.bladerf.rx.config.num_buffers = str2num(handles.num_buffers.String);
            handles.bladerf.rx.config.buffer_size = str2num(handles.buffer_size.String);
            handles.bladerf.rx.config.num_transfers = str2num(handles.num_transfers.String);

            hObject.String = 'Stop';
            handles.bladerf.rx.start;
            guidata(hObject, handles);

            start = cputime;
            update = 1;
            framerate = get_framerate(hObject);

            num_samples = get_num_samples(hObject);

            % We'll window our samples. However, we want to normalize
            % this to account for some of its inherent loss.
            win = blackmanharris(num_samples);
            win_norm = (1 / sum(abs(win) ./ num_samples) .* win);
            win_norm = win_norm ./ 4096;

            plots = get_plots(hObject);

            samples   = zeros(num_samples, 1);
            fft_data  = zeros(num_samples, 1);
            history   = zeros(num_samples, 1);

            prev_plot_id = -1;

            alpha = str2num(handles.fft_avg_alpha.String);
            if isempty(alpha) || alpha < 0.0 || alpha >= 1.0
                alpha = 0.0;
            end

            run = 1;
            setappdata(hObject.Parent.Parent, 'run', 1);

            while run == 1

                [samples(:), ~, ~, overrun] = ...
                    handles.bladerf.receive(num_samples);

                if overrun
                    print_overrun = get_print_overruns(hObject);
                    if print_overrun ~= 0
                        fprintf('Overrun @ t=%f\n', cputime - start);
                    end
                elseif update
                    update = 0;
                    id = handles.displaytype.Value;

                    % Since we're only displaying the GUI at some frame
                    % rate, there's no sense in constantly polling the
                    % user-supplied alpha for changes.
                    new_alpha = str2num(handles.fft_avg_alpha.String);
                    if ~isempty(alpha)
                        alpha = new_alpha;
                    end

                    % If we've changed plot types, we need to reset the
                    % FFT history, as it's only relevent for the associated
                    % mode
                    if id ~= prev_plot_id
                        history(:) = zeros(1, length(history)) + plots{id}.ymin;
                     end

                    switch plots{id}.name
                        case 'FFT (dB)'
                            fft_data(:) = 20*log10(abs(fftshift(fft(samples .* win_norm))));
                            history(:)  = history .* alpha + (1 - alpha) .* fft_data;
                            plots{id}.lines(1).YData(:) = history;

                        case 'FFT (linear)'
                            fft_data = abs(fftshift(fft(samples .* win_norm)));
                            history(:)  = history .* alpha + (1 - alpha) .* fft_data;
                            plots{id}.lines(1).YData(:) = history;

                        case 'Time (2-Channel)'
                            plots{id}.lines(1).YData(:) = real(samples);
                            plots{id}.lines(2).YData(:) = imag(samples);

                        case 'Time (XY)'
                            plots{id}.lines(1).XData(:) = real(samples);
                            plots{id}.lines(1).YData(:) = imag(samples);

                        otherwise
                            error('Invalid plot selection encountered');
                    end

                    drawnow;
                    tic;

                    prev_plot_id = id;
                    framerate = get_framerate(hObject);
                else
                    t = toc;
                    update = (t > (1/framerate));
                end

                run = getappdata(hObject.Parent.Parent, 'run');
            end

            handles.bladerf.rx.stop;
            figHandle = getappdata(hObject.Parent.Parent, 'ready_to_delete');
            if ~isempty(figHandle)
                delete(figHandle);
            end

        case 'Stop'
            setappdata(hObject.Parent.Parent, 'run', 0);
            handles.buffer_size.Enable = 'on';
            handles.num_buffers.Enable = 'on';
            handles.num_transfers.Enable = 'on';
            handles.devicelist.Enable = 'on';
            handles.xb200_attached.Enable = 'on';

            hObject.String = 'Start';
            guidata(hObject, handles);

        otherwise
            error(strcat('Unexpected button action: ', action))
    end
end

function lnagain_Callback(hObject, ~, handles)
    items = hObject.String;
    index = hObject.Value;

    %fprintf('GUI Request to set LNA gain to: %s\n', items{index})

    handles.bladerf.rx.lna = items{index};
    guidata(hObject, handles);
end

function lnagain_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = white;
    end
end

function vga1_Callback(hObject, ~, handles)
    val = str2num(hObject.String);
    if isempty(val)
        val = handles.bladerf.rx.vga1;
    end

    val = min(30, val);
    val = max(5, val);

    %fprintf('GUI request to set VGA1: %d\n', val);

    handles.bladerf.rx.vga1 = val;
    hObject.String = num2str(handles.bladerf.rx.vga1);
    guidata(hObject, handles);
end

function vga1_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function vga2_Callback(hObject, ~, handles)
    val = str2num(hObject.String);
    if isempty(val)
        val = handles.bladerf.rx.vga2;
    end

    val = min(30, val);
    val = max(0, val);

    %fprintf('GUI request to set VGA2: %d\n', val);

    handles.bladerf.rx.vga2 = val;
    hObject.String = num2str(handles.bladerf.rx.vga2);
    guidata(hObject, handles);
end

function vga2_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function bandwidth_Callback(hObject, ~, handles)
    values = hObject.String;
    index = hObject.Value;
    selected = str2num(values{index});

    bw = selected * 1.0e6;
    %fprintf('GUI request to set bandwidth to: %f\n', bw);

    handles.bladerf.rx.bandwidth = bw;
end

function bandwidth_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function corr_dc_i_Callback(hObject, ~, handles)
    val = str2num(hObject.String);
    if isempty(val)
        val = handles.bladerf.rx.corrections.dc_i;
    end

    val = min(2048, val);
    val = max(-2048, val);

    %fprintf('GUI request to set I DC correction to: %f\n', val)
    handles.bladerf.rx.corrections.dc_i = val;

    hObject.String = num2str(val);
end

function corr_dc_i_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function corr_dc_q_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function corr_dc_q_Callback(hObject, ~, handles)
    val = str2num(hObject.String);
    if isempty(val)
        val = handles.bladerf.rx.corrections.dc_q;
    end

    val = min(2048, val);
    val = max(-2048, val);

    %fprintf('GUI request to set IQ DC correction to: %f\n', val)
    handles.bladerf.rx.corrections.dc_q = val;

    hObject.String = num2str(val);
end

function corr_gain_Callback(hObject, ~, handles)
    val = str2num(hObject.String);
    if isempty(val)
        val = handles.bladerf.rx.corrections.gain;
    end

    val = max(-1.0, val);
    val = min(1.0, val);

    %fprintf('GUI request to set IQ gain correction to: %f\n', val)
    handles.bladerf.rx.corrections.gain = val;

    hObject.String = num2str(val);
end


function corr_gain_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function corr_phase_Callback(hObject, ~, handles)
    val = str2num(hObject.String);
    if isempty(val)
        val = handles.bladerf.rx.corrections.phase;
    end

    %fprintf('GUI request to set phase correction to: %f\n', val)
    handles.bladerf.rx.corrections.phase = val;

    val = handles.bladerf.rx.corrections.phase;
    hObject.String = num2str(val);
end

function corr_phase_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function samplerate_Callback(hObject, ~, handles)
    val = str2num(hObject.String);
    if isempty(val)
        val = handles.bladerf.rx.samplerate;
    end

    val = min(40e6, val);
    val = max(160e3, val);

    %fprintf('GUI request to set samplerate to: %f\n', val);

    handles.bladerf.rx.samplerate = val;
    val = handles.bladerf.rx.samplerate;
    hObject.String = num2str(val);

    update_plot_axes(hObject, handles);
end

function samplerate_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function frequency_Callback(hObject, ~, handles)
    val = str2num(hObject.String);
    if isempty(val)
        val = handles.bladerf.rx.frequency;
    end

    val = min(3.8e9, val);
    val = max(handles.bladerf.rx.min_frequency, val);

    %fprintf('GUI request to set frequency: %d\n', val);

    handles.bladerf.rx.frequency = val;
    hObject.String = num2str(val);
    hObject.Value = val;

    update_plot_axes(hObject, handles);
end

function frequency_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function devicelist_Callback(hObject, ~, handles)
    if handles.xb200_attached.Value == true
        xb = 'XB200';
    else
        xb = [];
    end

    items = hObject.String;
    index = hObject.Value;
    devstring = items{index};
    handles.bladerf.delete;
    guidata(hObject, handles);
    handles.bladerf = bladeRF(devstring, [], xb);
    read_device_parameters(hObject, handles);
    guidata(hObject, handles);
end

function xb200_attached_Callback(hObject, ~, handles)
    if hObject.Value == true
        xb = 'XB200';
    else
        xb = [];
    end

    items = handles.devicelist.String;
    index = handles.devicelist.Value;
    devstring = items{index};
    handles.bladerf.delete;
    guidata(hObject, handles);
    handles.bladerf = bladeRF(devstring, [], xb);
    read_device_parameters(hObject, handles);
    guidata(hObject, handles);
end


function devicelist_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
    devs = bladeRF.devices;
    list = cell(1, length(devs));
    for idx=1:length(devs)
        switch devs(idx).backend
            case 'BLADERF_BACKEND_LIBUSB'
                backend = 'libusb';
            case 'BLADERF_BACKEND_CYPRESS'
                backend = 'cypress';
            otherwise
                disp('Not sure which backend is being used');
                backend = '*';
        end
        list{idx} = strcat(backend, ':serial=', devs(idx).serial);
    end
    hObject.String = list;
end

function figure1_CloseRequestFcn(hObject, ~, ~)
    running = getappdata(hObject.Parent, 'run');
    if running == 1
        % Our hackish receive loop is still running. Flag it to shut
        % down and have it take care of the final delete().
        setappdata(hObject.Parent, 'run', 0);
        setappdata(hObject.Parent, 'ready_to_delete', hObject)
    else
        % We can shut down now.
        delete(hObject);
    end
end

function num_buffers_Callback(hObject, ~, handles)
    num_buffers = str2num(hObject.String);
    if isempty(num_buffers)
        num_buffers = handles.bladerf.rx.config.num_buffers;
    end

    num_buffers = max(2, num_buffers);
    handles.bladerf.rx.config.num_buffers = num_buffers;
    hObject.String = num2str(num_buffers);
end

function num_buffers_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function buffer_size_Callback(hObject, ~, handles)
    buffer_size = str2num(hObject.String);
    if isempty(buffer_size);
        buffer_size = handles.bladerf.rx.config.buffer_size;
    end

    if (mod(buffer_size, 1024) ~= 0)
        buffer_size = max(1024, floor((buffer_size / 1024)) * 1024);
    end

    handles.bladerf.rx.config.buffer_size = buffer_size;
    hObject.String = num2str(handles.bladerf.rx.config.buffer_size);
end

function buffer_size_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function num_transfers_Callback(hObject, ~, handles)
	num_transfers = str2num(hObject.String);
    if isempty(num_transfers)
        num_transfers = handles.bladerf.rx.config.num_transfers;
    end

    num_buffers = handles.bladerf.rx.config.num_buffers;

    if num_transfers < 1
        num_transfers = 1;
    elseif num_transfers > floor(num_buffers / 2)
        num_transfers = max(1, floor(num_buffers/2));
    end

    handles.bladerf.rx.config.num_transfers = num_transfers;
    hObject.String = num2str(handles.bladerf.rx.config.num_transfers);
end

function num_transfers_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function print_overruns_Callback(hObject, ~, ~)
    if hObject.Value ~= 0
        set_print_overruns(hObject, 1);
    else
        set_print_overruns(hObject, 0);
    end
end

function fft_avg_alpha_Callback(~, ~, ~)
end

function fft_avg_alpha_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        hObject.BackgroundColor = 'white';
    end
end

function framerate_Callback(hObject, ~, ~)
    fps = str2num(hObject.String);
    if isempty(fps) == true
        fps = get_framerate(hObject);
    elseif fps < 1
        fps = 1;
    end

    fps = round(fps);
    hObject.String = num2str(fps);
    set_framerate(hObject, fps);
end

function framerate_CreateFcn(hObject, ~, ~)
    if ispc && isequal(get(hObject,'BackgroundColor'), get(0,'defaultUicontrolBackgroundColor'))
        set(hObject,'BackgroundColor','white');
    end
end
