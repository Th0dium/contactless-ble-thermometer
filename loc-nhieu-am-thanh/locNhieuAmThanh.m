function locNhieuAmThanh
% LOCNHIEUAMTHANH  App "Lọc nhiễu âm thanh": thêm nhiễu với SNR cho trước,
% lọc bằng Notch / Thông dải / Trừ phổ, rồi đo SNR trước và sau lọc.
% Bấm Run để chạy. Cần MATLAB R2019a+ và Signal Processing Toolbox.

% ===== Trạng thái dùng chung giữa các callback =====
x = [];                       % tín hiệu nguồn đã chuẩn hóa (chưa thêm im lặng)
fs = 16000;                   % tần số lấy mẫu của nguồn
xSach = []; xOn = []; xLoc = [];
fsKQ = fs;                    % fs của kết quả đã xử lý (dùng khi nghe/vẽ)

% ===== Giao diện =====
fig = uifigure('Name','Lọc nhiễu âm thanh','Position',[80 60 1250 760]);
gChinh = uigridlayout(fig,[1 2]);
gChinh.ColumnWidth = {270,'1x'};

pnl = uipanel(gChinh,'Title','Điều khiển');
gl = uigridlayout(pnl,[18 1]);
gl.RowHeight = {30,22,22,26,22,40,22,26,22,40,34,28,28,28,28,22,22,22};
gl.RowSpacing = 6;

uibutton(gl,'Text','Load file','ButtonPushedFcn',@(~,~) onLoad());
lblNguon = uilabel(gl,'Text','Nguồn: tín hiệu mẫu (chưa load file)');
uilabel(gl,'Text','Loại nhiễu');
ddNhieu = uidropdown(gl,'Items',{'Nhiễu trắng','Tiếng ù 50 Hz','Cả hai'}, ...
    'ItemsData',{'trang','u50','cahai'});
lblSNR = uilabel(gl,'Text','SNR đầu vào: 5.0 dB');
sldSNR = uislider(gl,'Limits',[-5 20],'Value',5,'ValueChangedFcn',@(~,~) capNhatNhan());
uilabel(gl,'Text','Phương pháp lọc');
ddLoc = uidropdown(gl,'Items',{'Notch 50 Hz','Thông dải 300-3400 Hz','Trừ phổ'}, ...
    'ItemsData',{'notch','thongdai','trupho'},'ValueChangedFcn',@(~,~) capNhatNhan());
lblAlpha = uilabel(gl,'Text','alpha (trừ phổ): 2.0');
sldAlpha = uislider(gl,'Limits',[1 4],'Value',2,'Enable','off', ...
    'ValueChangedFcn',@(~,~) capNhatNhan());
uibutton(gl,'Text','Xử lý','FontWeight','bold','ButtonPushedFcn',@(~,~) onXuLy());
uibutton(gl,'Text','Nghe gốc','ButtonPushedFcn',@(~,~) onNghe('goc'));
uibutton(gl,'Text','Nghe có ồn','ButtonPushedFcn',@(~,~) onNghe('on'));
uibutton(gl,'Text','Nghe đã lọc','ButtonPushedFcn',@(~,~) onNghe('loc'));
uibutton(gl,'Text','Dừng','ButtonPushedFcn',@(~,~) dungAm());
lblTruoc = uilabel(gl,'Text','SNR trước: - dB');
lblSau = uilabel(gl,'Text','SNR sau: - dB');
lblCaiThien = uilabel(gl,'Text','Cải thiện: - dB','FontWeight','bold');

gAx = uigridlayout(gChinh,[3 1]);
ax1 = uiaxes(gAx); title(ax1,'Dạng sóng'); xlabel(ax1,'Thời gian (s)');
ax2 = uiaxes(gAx); title(ax2,'Phổ công suất (pwelch)');
xlabel(ax2,'Tần số (Hz)'); ylabel(ax2,'dB/Hz');
ax3 = uiaxes(gAx); title(ax3,'Spectrogram tín hiệu sau lọc');
xlabel(ax3,'Thời gian (s)'); ylabel(ax3,'Tần số (Hz)');

% ===== Callback =====
    function capNhatNhan()
        % Hiện giá trị slider; alpha chỉ dùng cho trừ phổ
        lblSNR.Text = sprintf('SNR đầu vào: %.1f dB', sldSNR.Value);
        lblAlpha.Text = sprintf('alpha (trừ phổ): %.1f', sldAlpha.Value);
        if strcmp(ddLoc.Value,'trupho')
            sldAlpha.Enable = 'on';
        else
            sldAlpha.Enable = 'off';
        end
    end

    function onLoad()
        [f,p] = uigetfile({'*.wav','File WAV (*.wav)'},'Chọn file âm thanh');
        if isequal(f,0), return; end          % người dùng bấm Cancel
        try
            [y,fsDoc] = audioread(fullfile(p,f));
        catch ME
            uialert(fig,['Không đọc được file: ' ME.message],'Lỗi');
            return;
        end
        % Kiểm tra tần số lấy mẫu: dải thông 3400 Hz phải nhỏ hơn fs/2
        if fsDoc < 8000
            uialert(fig,sprintf('fs = %d Hz quá thấp (cần >= 8000 Hz).',fsDoc),'Lỗi');
            return;
        end
        % File stereo: chỉ lấy kênh 1
        if size(y,2) > 1
            y = y(:,1);
            uialert(fig,'File stereo - chỉ dùng kênh 1.','Thông báo','Icon','info');
        end
        % Tín hiệu quá ngắn
        if numel(y) < fsDoc
            uialert(fig,'Tín hiệu quá ngắn (cần >= 1 giây).','Lỗi');
            return;
        end
        if max(abs(y)) == 0
            uialert(fig,'File toàn im lặng.','Lỗi');
            return;
        end
        x = y / max(abs(y));                  % chuẩn hóa về [-1, 1]
        fs = fsDoc;
        lblNguon.Text = ['Nguồn: ' f];
    end

    function onXuLy()
        % Chưa load file thì dùng tín hiệu mẫu
        if isempty(x)
            fs = 16000;
            x = taoTinHieuMau(fs);
            uialert(fig,'Chưa load file - dùng tín hiệu mẫu (16 kHz, 2 giây).', ...
                'Thông báo','Icon','info');
        end
        try
            rng(0);                            % cố định nhiễu để so sánh tham số
            % B1: thêm 0.5 s im lặng ở đầu để học phổ nhiễu
            nIm = round(0.5*fs);
            xs = [zeros(nIm,1); x];
            % B2: tạo nhiễu và cộng vào
            xn = xs + taoNhieu(xs, fs, ddNhieu.Value, sldSNR.Value);
            % B3: lọc
            switch ddLoc.Value
                case 'notch'
                    xl = locNotch(xn, fs, 35);
                case 'thongdai'
                    xl = locThongDai(xn, fs);
                otherwise
                    xl = truPho(xn, fs, sldAlpha.Value, 0.02, nIm);
            end
            % B4: cắt về cùng độ dài
            N = min([numel(xs) numel(xn) numel(xl)]);
            xSach = xs(1:N); xOn = xn(1:N); xLoc = xl(1:N);
            fsKQ = fs;
            % B5: đo SNR
            snrTruoc = tinhSNR(xSach, xOn);
            snrSau = tinhSNR(xSach, xLoc);
            lblTruoc.Text = sprintf('SNR trước: %.2f dB', snrTruoc);
            lblSau.Text = sprintf('SNR sau: %.2f dB', snrSau);
            lblCaiThien.Text = sprintf('Cải thiện: %.2f dB', snrSau - snrTruoc);
            veDoThi();
        catch ME
            uialert(fig, ME.message, 'Lỗi xử lý');
        end
    end

    function onNghe(loai)
        switch loai
            case 'goc', sig = xSach;
            case 'on',  sig = xOn;
            otherwise,  sig = xLoc;
        end
        if isempty(sig)
            uialert(fig,'Chưa có tín hiệu - bấm "Xử lý" trước.','Thông báo','Icon','warning');
            return;
        end
        phatAm(sig, fsKQ);
    end

    function veDoThi()
        % (1) Dạng sóng
        t = (0:numel(xSach)-1)'/fsKQ;
        plot(ax1, t, xOn, t, xLoc, t, xSach);
        legend(ax1, {'Có nhiễu','Sau lọc','Sạch'});
        xlim(ax1, [0 t(end)]);
        % (2) Phổ chồng 3 tín hiệu
        [P,f] = pwelch([xSach xOn xLoc], hamming(1024), 512, 1024, fsKQ);
        plot(ax2, f, 10*log10(P + eps));
        legend(ax2, {'Sạch','Có nhiễu','Sau lọc'});
        xlim(ax2, [0 fsKQ/2]);
        % (3) Spectrogram sau lọc: lấy output rồi tự vẽ lên UIAxes
        [s,f2,t2] = spectrogram(xLoc, hamming(512), 384, 512, fsKQ);
        SdB = 10*log10(abs(s).^2 + eps);
        imagesc(ax3, t2, f2, SdB);
        ax3.YDir = 'normal';
        colormap(ax3, jet);
        ax3.CLim = [max(SdB(:))-80, max(SdB(:))];
        xlim(ax3, [t2(1) t2(end)]); ylim(ax3, [0 fsKQ/2]);
    end
end

% ===== Hàm con =====
function x = taoTinHieuMau(fs)
% Tổng 3 sóng sin trong dải thoại, điều chế biên độ 3 Hz cho giống âm tiết
t = (0:2*fs-1)'/fs;
bao = 0.5*(1 + sin(2*pi*3*t));
x = bao .* (0.6*sin(2*pi*440*t) + 0.3*sin(2*pi*1000*t) + 0.2*sin(2*pi*2000*t));
x = x / max(abs(x));
end

function n = taoNhieu(xs, fs, loai, snrDb)
% Công suất nhiễu theo SNR mong muốn: Pn = P_tín_hiệu / 10^(SNR/10)
Pn = mean(xs.^2) / 10^(snrDb/10);
N = numel(xs);
t = (0:N-1)'/fs;
switch loai
    case 'trang'
        n = sqrt(Pn)*randn(N,1);
    case 'u50'
        n = sqrt(2*Pn)*sin(2*pi*50*t);         % sin biên độ A có công suất A^2/2
    otherwise
        % Cả hai: mỗi loại một nửa công suất (Pn/2)
        n = sqrt(Pn/2)*randn(N,1) + sqrt(Pn)*sin(2*pi*50*t);
end
end

function y = locNotch(x, fs, Q)
% Notch 50 Hz: iirnotch nhận băng thông bw = w0/Q (tần số chuẩn hóa)
w0 = 50/(fs/2);
[b,a] = iirnotch(w0, w0/Q);
y = filtfilt(b, a, x);                         % lọc 2 chiều, không lệch pha
end

function y = locThongDai(x, fs)
% Butterworth bậc 4, dải 300-3400 Hz; dạng SOS cho ổn định số
[z,p,k] = butter(4, [300 3400]/(fs/2), 'bandpass');
[sos,g] = zp2sos(z, p, k);
y = filtfilt(sos, g, x);
end

function y = truPho(x, fs, alpha, beta, nIm)
% Trừ phổ: STFT -> trừ biên độ nhiễu -> ghép lại bằng ISTFT
nfft = 512; ov = 384; hop = nfft - ov;
win = hamming(nfft, 'periodic');               % periodic để ISTFT tái tạo đúng
S = stft(x, fs, 'Window', win, 'OverlapLength', ov, 'FFTLength', nfft);
mag = abs(S);
pha = angle(S);
% Ước lượng phổ nhiễu: trung bình các khung nằm trọn trong 0.5 s đầu
nKhung = max(1, floor((nIm - nfft)/hop) + 1);
noiseMag = mean(mag(:,1:nKhung), 2);
% Trừ biên độ, giữ sàn beta*mag để tránh giá trị âm
magClean = max(mag - alpha*noiseMag, beta*mag);
% Giữ nguyên pha, ghép lại và lấy phần thực
y = istft(magClean .* exp(1j*pha), fs, 'Window', win, ...
    'OverlapLength', ov, 'FFTLength', nfft);
y = real(y(:));
end

function snr = tinhSNR(ref, est)
% SNR = 10*log10(năng lượng tín hiệu sạch / năng lượng sai số)
snr = 10*log10(sum(ref.^2) / sum((ref - est).^2));
end

function phatAm(sig, fs)
% Dừng âm đang phát, chuẩn hóa biên độ rồi phát (tránh vỡ tiếng)
clear sound
sound(0.95*sig/max(abs(sig)), fs);
end

function dungAm()
clear sound
end
