// SerialForWindowsTerminal.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "SerialForWindowsTerminal.h"
#include <vector>
#include <string>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/windows/stream_handle.hpp>

#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE hInstance;
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    SettingFunc(HWND, UINT, WPARAM, LPARAM);


using PortsArray = std::vector<std::pair<std::wstring, int>>;
static PortsArray GetAllPorts(void)
{
    PortsArray ports;
    HKEY hRegKey;
    int nCount = 0;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Hardware\\DeviceMap\\SerialComm", 0, KEY_READ, &hRegKey) == ERROR_SUCCESS)
    {
        while (true)
        {
            TCHAR szName[MAX_PATH] = { 0 };
            TCHAR szPort[MAX_PATH] = { 0 };
            DWORD nValueSize = MAX_PATH - 1;
            DWORD nDataSize = MAX_PATH - 1;
            DWORD nType;

            if (::RegEnumValue(hRegKey, nCount, szName, &nValueSize, NULL, &nType, (LPBYTE)szPort, &nDataSize) == ERROR_NO_MORE_ITEMS)
            {
                break;
            }
            std::wstring name(szName);
            auto idx = name.find_last_of('\\');
            if (idx != name.npos)
            {
                name = name.substr(idx + 1);
            }
            name += L" (";
            name += szPort;
            name += L")";
            ports.push_back(std::make_pair(name, (int)std::wcstoul(szPort + 3, nullptr, 10)));
            nCount++;
        }
        ::RegCloseKey(hRegKey);
    }
    return ports;
}

static void UpdatePortControl(HWND hDlg)
{
    auto allPorts = GetAllPorts();
    auto hWndPort = GetDlgItem(hDlg, IDC_COMBO_PORT);
    ComboBox_ResetContent(hWndPort);
    for (auto port : allPorts)
    {
        auto index = ComboBox_AddString(hWndPort, port.first.c_str());
        ComboBox_SetItemData(hWndPort, index, port.second);
    }
}

static void CenterParentWindow(HWND hWnd)
{
    RECT rcDlg;
    ::GetWindowRect(hWnd, &rcDlg);
    RECT rcParent;
    HWND hWndParent = GetParent(hWnd);
    GetClientRect(hWndParent, &rcParent);
    POINT ptParentInScreen;
    ptParentInScreen.x = rcParent.left;
    ptParentInScreen.y = rcParent.top;
    ::ClientToScreen(hWndParent, (LPPOINT)&ptParentInScreen);
    SetWindowPos(
        hWnd,
        NULL,
        ptParentInScreen.x + (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2,
        ptParentInScreen.y + (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2,
        0,
        0,
        SWP_NOZORDER | SWP_NOSIZE);
}

typedef struct
{
    DWORD Serial;
    DWORD BaudRate;
    DWORD WordLength;
    DWORD StopBit;
    DWORD Parity;
    DWORD FlowControl;
    DWORD Encoding;
}SERIAL_CONFIG;

static std::string ConvertEncoding(const std::string& text, DWORD from, DWORD to)
{
    if (text.empty() || from == to)
        return text;

    std::wstring wstr;
    int wlen = MultiByteToWideChar(from, 0, text.c_str(), (int)text.size(), NULL, 0);
    if (wlen > 0)
    {
        wstr.resize(wlen);
        MultiByteToWideChar(from, 0, text.c_str(), (int)text.size(), &wstr[0], wlen);
    }

    std::string result;
    int mblen = WideCharToMultiByte(to, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (mblen > 0)
    {
        result.resize(mblen);
        WideCharToMultiByte(to, 0, wstr.c_str(), (int)wstr.size(), &result[0], mblen, NULL, NULL);
    }
    return result;
}

static SERIAL_CONFIG ReadSerialConfig()
{
    HKEY hKey;
    SERIAL_CONFIG cfg;
    cfg.Serial = 0;
    cfg.BaudRate = 9600;
    cfg.WordLength = 8;
    cfg.StopBit = ONESTOPBIT;
    cfg.Parity = NOPARITY;
    cfg.FlowControl = 0;
    cfg.Encoding = CP_UTF8;
    if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\SerialForWindowsTerminal", 0, KEY_READ, &hKey))
    {
        DWORD dwSize = sizeof(DWORD);
        DWORD dwType = REG_DWORD;

        ::RegQueryValueEx(hKey, L"Serial", 0, &dwType, (LPBYTE)&cfg.Serial, &dwSize);
        ::RegQueryValueEx(hKey, L"BaudRate", 0, &dwType, (LPBYTE)&cfg.BaudRate, &dwSize);
        ::RegQueryValueEx(hKey, L"WordLength", 0, &dwType, (LPBYTE)&cfg.WordLength, &dwSize);
        ::RegQueryValueEx(hKey, L"StopBit", 0, &dwType, (LPBYTE)&cfg.StopBit, &dwSize);
        ::RegQueryValueEx(hKey, L"Parity", 0, &dwType, (LPBYTE)&cfg.Parity, &dwSize);
        ::RegQueryValueEx(hKey, L"FlowControl", 0, &dwType, (LPBYTE)&cfg.FlowControl, &dwSize);
        ::RegQueryValueExW(hKey, L"Encoding", 0, &dwType, (LPBYTE)&cfg.Encoding, &dwSize);
        ::RegCloseKey(hKey);
    }
    return cfg;
}

static void WriteSerialConfig(const SERIAL_CONFIG& cfg)
{
    HKEY hKey;
    if (ERROR_SUCCESS == ::RegCreateKey(HKEY_CURRENT_USER, L"SOFTWARE\\SerialForWindowsTerminal", &hKey))
    {
        DWORD dwSize = sizeof(DWORD);
        DWORD dwType = REG_DWORD;

        ::RegSetValueEx(hKey, L"Serial", 0, dwType, (CONST LPBYTE) & cfg.Serial, dwSize);
        ::RegSetValueEx(hKey, L"BaudRate", 0, dwType, (CONST LPBYTE) & cfg.BaudRate, dwSize);
        ::RegSetValueEx(hKey, L"WordLength", 0, dwType, (CONST LPBYTE) & cfg.WordLength, dwSize);
        ::RegSetValueEx(hKey, L"StopBit", 0, dwType, (CONST LPBYTE) & cfg.StopBit, dwSize);
        ::RegSetValueEx(hKey, L"Parity", 0, dwType, (CONST LPBYTE) & cfg.Parity, dwSize);
        ::RegSetValueEx(hKey, L"FlowControl", 0, dwType, (CONST LPBYTE) & cfg.FlowControl, dwSize);
        ::RegSetValueEx(hKey, L"Encoding", 0, dwType, (CONST LPBYTE) & cfg.Encoding, dwSize);
        ::RegCloseKey(hKey);
    }
}

static boost::system::error_code InitializeSerialPort(boost::asio::serial_port& serialPort,const SERIAL_CONFIG& cfg, boost::system::error_code& ec)
{
    serialPort.set_option(boost::asio::serial_port::baud_rate(cfg.BaudRate), ec);
    if (ec)
        return ec;
    serialPort.set_option(boost::asio::serial_port::character_size(cfg.WordLength), ec);
    if (ec)
        return ec;
    switch (cfg.StopBit)
    {
    case 0:
        serialPort.set_option(boost::asio::serial_port::stop_bits(boost::asio::serial_port::stop_bits::one), ec);
        break;
    case 1:
        serialPort.set_option(boost::asio::serial_port::stop_bits(boost::asio::serial_port::stop_bits::onepointfive), ec);
        break;
    case 2:
        serialPort.set_option(boost::asio::serial_port::stop_bits(boost::asio::serial_port::stop_bits::two), ec);
        break;
    default:
        serialPort.set_option(boost::asio::serial_port::stop_bits(boost::asio::serial_port::stop_bits::one), ec);
        break;
    }
    if (ec)
        return ec;

    switch (cfg.Parity)
    {
    case 0:
        serialPort.set_option(boost::asio::serial_port::parity(boost::asio::serial_port::parity::none), ec);
        break;
    case 1:
        serialPort.set_option(boost::asio::serial_port::parity(boost::asio::serial_port::parity::odd), ec);
        break;
    case 2:
        serialPort.set_option(boost::asio::serial_port::parity(boost::asio::serial_port::parity::even), ec);
        break;
    default:
        serialPort.set_option(boost::asio::serial_port::parity(boost::asio::serial_port::parity::none), ec);
        break;
    }
    if (ec)
        return ec;

    switch (cfg.FlowControl)
    {
    case 0:
        serialPort.set_option(boost::asio::serial_port::flow_control(boost::asio::serial_port::flow_control::none), ec);
        break;
    case 1:
        serialPort.set_option(boost::asio::serial_port::flow_control(boost::asio::serial_port::flow_control::software), ec);
        break;
    case 2:
        serialPort.set_option(boost::asio::serial_port::flow_control(boost::asio::serial_port::flow_control::hardware), ec);
        break;
    default:
        serialPort.set_option(boost::asio::serial_port::flow_control(boost::asio::serial_port::flow_control::none), ec);
        break;
    }
    
    return ec;
}

template <class TStream1, class TStream2>
static void DoStreamToStream(TStream1& stream1, TStream2& stream2, std::vector<uint8_t>& buffer)
{
    stream1.async_read_some(
        boost::asio::buffer(buffer.data(), buffer.size()),
        [&stream1, &stream2, &buffer](const boost::system::error_code& ec, std::size_t bytes_transferred)
        {
            if (ec)
            {
                std::cerr << "\033[31m" << "error : " << ec.message() << "\033[0m" << std::endl;
            }
            else
            {
                boost::asio::async_write(
                    stream2,
                    boost::asio::const_buffer(buffer.data(), bytes_transferred),
                    [&stream1, &stream2, &buffer](const boost::system::error_code& ec, std::size_t bytes_transferred)
                    {
                        if (ec)
                        {
                            std::cerr << "\033[31m" << "error : " << ec.message() << "\033[0m" << std::endl;
                        }
                        else
                        {
                            DoStreamToStream(stream1, stream2, buffer);
                        }
                    }
                );
            }
        }
    );
}

static boost::system::error_code DoWork(boost::asio::io_context& ioctx, boost::asio::serial_port& serialPort)
{
    boost::system::error_code ec;
    boost::asio::windows::stream_handle stdinput(ioctx);
    boost::asio::windows::stream_handle stdoutput(ioctx);
    const auto kBufferSize = 1024;

    // 读取配置（包含编码设置）
    auto cfg = ReadSerialConfig();

    // 获取控制台当前输入和输出编码
    UINT consoleInputCP = GetConsoleCP();
    UINT consoleOutputCP = GetConsoleOutputCP();
    const DWORD targetEncoding = cfg.Encoding;

    // 编码转换函数
    auto convertEncoding = [](const std::vector<uint8_t>& input, DWORD from, DWORD to) -> std::vector<uint8_t> {
        if (input.empty() || from == to) {
            return input;
        }

        // 将输入数据转换为宽字符
        std::wstring wstr;
        int wlen = MultiByteToWideChar(from, 0,
            reinterpret_cast<const char*>(input.data()),
            static_cast<int>(input.size()),
            NULL, 0);
        if (wlen <= 0) return input;

        wstr.resize(wlen);
        if (MultiByteToWideChar(from, 0,
            reinterpret_cast<const char*>(input.data()),
            static_cast<int>(input.size()),
            &wstr[0], wlen) == 0) {
            return input;
        }

        // 将宽字符转换为目标编码
        std::vector<uint8_t> result;
        int mblen = WideCharToMultiByte(to, 0, wstr.data(), static_cast<int>(wstr.size()),
            NULL, 0, NULL, NULL);
        if (mblen <= 0) return input;

        result.resize(mblen);
        if (WideCharToMultiByte(to, 0, wstr.data(), static_cast<int>(wstr.size()),
            reinterpret_cast<char*>(result.data()), mblen, NULL, NULL) == 0) {
            return input;
        }
        return result;
        };

    // 发送转换: 控制台输入编码 -> 目标编码
    auto convertForSend = [&](const std::vector<uint8_t>& input) {
        return convertEncoding(input, consoleInputCP, targetEncoding);
        };

    // 接收转换: 目标编码 -> 控制台输出编码
    auto convertForRecv = [&](const std::vector<uint8_t>& input) {
        return convertEncoding(input, targetEncoding, consoleOutputCP);
        };

    // 缓冲区
    std::vector<uint8_t> serialPortRecvBuffer;
    std::vector<uint8_t> serialPortSendBuffer;
    serialPortRecvBuffer.resize(kBufferSize);
    serialPortSendBuffer.resize(kBufferSize);

    auto conin = CreateFile(L"CONIN$", FILE_GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
    auto conout = CreateFile(L"CONOUT$", FILE_GENERIC_WRITE, FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

    if (stdinput.assign(conin, ec))
        return ec;

    if (stdoutput.assign(conout, ec))
        return ec;

    // 使用 std::function 定义处理器类型
    using StreamHandler = std::function<void()>;

    // 创建处理器对象
    StreamHandler recvHandler;
    StreamHandler sendHandler;

    // 接收方向：串口 -> 控制台
    recvHandler = [&]() {
        serialPort.async_read_some(
            boost::asio::buffer(serialPortRecvBuffer),
            [&](const boost::system::error_code& ec, size_t bytes_transferred) {
                if (ec) {
                    std::cerr << "\033[31merror: " << ec.message() << "\033[0m\n";
                    return;
                }

                // 复制接收到的数据
                std::vector<uint8_t> received(
                    serialPortRecvBuffer.begin(),
                    serialPortRecvBuffer.begin() + bytes_transferred
                );

                // 应用编码转换
                auto converted = convertForRecv(received);

                // 写入转换后的数据
                boost::asio::async_write(
                    stdoutput,
                    boost::asio::buffer(converted),
                    [&](const boost::system::error_code& ec, size_t) {
                        if (ec) {
                            std::cerr << "\033[31merror: " << ec.message() << "\033[0m\n";
                            return;
                        }

                        // 继续处理下一批数据
                        recvHandler();
                    }
                );
            }
        );
        };

    // 发送方向：控制台 -> 串口
    sendHandler = [&]() {
        stdinput.async_read_some(
            boost::asio::buffer(serialPortSendBuffer),
            [&](const boost::system::error_code& ec, size_t bytes_transferred) {
                if (ec) {
                    std::cerr << "\033[31merror: " << ec.message() << "\033[0m\n";
                    return;
                }

                // 复制接收到的数据
                std::vector<uint8_t> received(
                    serialPortSendBuffer.begin(),
                    serialPortSendBuffer.begin() + bytes_transferred
                );

                // 应用编码转换
                auto converted = convertForSend(received);

                // 写入转换后的数据
                boost::asio::async_write(
                    serialPort,
                    boost::asio::buffer(converted),
                    [&](const boost::system::error_code& ec, size_t) {
                        if (ec) {
                            std::cerr << "\033[31merror: " << ec.message() << "\033[0m\n";
                            return;
                        }

                        // 继续处理下一批数据
                        sendHandler();
                    }
                );
            }
        );
        };

    // 启动两个方向的处理器
    recvHandler();
    sendHandler();

    // 运行IO上下文
    ioctx.run();
    return ec;
}

int wmain(int argc, const WCHAR* args[])
{
    boost::system::error_code ec;
    boost::asio::io_context ioctx;
    boost::asio::serial_port serialPort(ioctx);
    hInstance = GetModuleHandle(nullptr);

    DWORD consoleMode = 0;
    auto conin = GetStdHandle(STD_INPUT_HANDLE);
    auto conout = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(conin, &consoleMode);
    consoleMode |= ENABLE_MOUSE_INPUT;
    consoleMode &= ~ENABLE_ECHO_INPUT;
    consoleMode &= ~ENABLE_PROCESSED_INPUT;
    consoleMode &= ~ENABLE_LINE_INPUT;
    consoleMode |= ENABLE_QUICK_EDIT_MODE;
    consoleMode |= ENABLE_WINDOW_INPUT;
    consoleMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    SetConsoleMode(conin, consoleMode);

    GetConsoleMode(conout, &consoleMode);
    SetConsoleMode(conout, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);

    while (true)
    {
        auto hWndParent = ::GetForegroundWindow();
        if (hWndParent == nullptr)
            hWndParent = GetConsoleWindow();
        if (DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_SETTING_DIALOG), hWndParent, SettingFunc, 1) == IDOK)
        {
            auto cfg = ReadSerialConfig();
            auto portName = std::string("COM") + std::to_string(cfg.Serial);
            if (serialPort.open(portName, ec))
            {
                std::cerr << "\033[31m" << "can not open " << portName << "\033[0m" <<std::endl;
                std::cerr << "\033[31m" << "error : " << ec.message() << "\033[0m" << std::endl;
                continue;
            }
            if (InitializeSerialPort(serialPort, cfg, ec))
            {
                std::cerr << "\033[31m" << "can not initialize " << portName << "\033[0m" << std::endl;
                std::cerr << "\033[31m" << "error : " << ec.message() << "\033[0m" << std::endl;
                continue;
            }
            break;
        }
        else
        {
            return ERROR_CANCELLED;
        }
    }
    ec = DoWork(ioctx, serialPort);
    if (ec)
    {
        std::cerr << "\033[31m" << "error : " << ec.message() << "\033[0m" << std::endl;
        return ec.value();
    }
    return ERROR_SUCCESS;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        CenterParentWindow(hDlg);
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK SettingFunc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
    {
        CenterParentWindow(hDlg);
        auto cfg = ReadSerialConfig();
        auto hWndPort = GetDlgItem(hDlg, IDC_COMBO_PORT);
        UpdatePortControl(hDlg);
        auto portCount = ComboBox_GetCount(hWndPort);
        if (cfg.Serial != 0)
        {
            for (int i = 0; i < portCount; ++i)
            {
                auto com = (int)ComboBox_GetItemData(hWndPort, i);
                if (com == cfg.Serial)
                {
                    ComboBox_SetCurSel(hWndPort, i);
                    break;
                }
            }
        }
        else
        {
            ComboBox_SetCurSel(hWndPort, 0);
        }

        auto hWndBaudRate = GetDlgItem(hDlg, IDC_COMBO_SPEED);
        ComboBox_AddString(hWndBaudRate, L"50"); 
        ComboBox_AddString(hWndBaudRate, L"75");
        ComboBox_AddString(hWndBaudRate, L"100");
        ComboBox_AddString(hWndBaudRate, L"105");
        ComboBox_AddString(hWndBaudRate, L"300");
        ComboBox_AddString(hWndBaudRate, L"600");
        ComboBox_AddString(hWndBaudRate, L"1200");
        ComboBox_AddString(hWndBaudRate, L"2400");
        ComboBox_AddString(hWndBaudRate, L"4800");
        ComboBox_AddString(hWndBaudRate, L"9600");
        ComboBox_AddString(hWndBaudRate, L"19200");
        ComboBox_AddString(hWndBaudRate, L"38400");
        ComboBox_AddString(hWndBaudRate, L"57600");
        ComboBox_AddString(hWndBaudRate, L"115200");
        ComboBox_AddString(hWndBaudRate, L"128000");
        ComboBox_AddString(hWndBaudRate, L"256000");
        ComboBox_SetText(hWndBaudRate, std::to_wstring(cfg.BaudRate).c_str());

        auto hWndWordLength = GetDlgItem(hDlg, IDC_COMBO_WORD);
        ComboBox_AddString(hWndWordLength, L"4");
        ComboBox_AddString(hWndWordLength, L"5");
        ComboBox_AddString(hWndWordLength, L"6");
        ComboBox_AddString(hWndWordLength, L"7");
        ComboBox_AddString(hWndWordLength, L"8");
        ComboBox_AddString(hWndWordLength, L"9");
        ComboBox_AddString(hWndWordLength, L"10");
        ComboBox_SetCurSel(hWndWordLength, (int)(cfg.WordLength - 4));

        auto hWndStopBit = GetDlgItem(hDlg, IDC_COMBO_STOP);
        ComboBox_AddString(hWndStopBit, L"1");
        ComboBox_AddString(hWndStopBit, L"1.5");
        ComboBox_AddString(hWndStopBit, L"2");
        ComboBox_SetCurSel(hWndStopBit, (int)(cfg.StopBit));

        auto hWndParity = GetDlgItem(hDlg, IDC_COMBO_PARITY);
        ComboBox_AddString(hWndParity, L"无");
        ComboBox_AddString(hWndParity, L"奇");
        ComboBox_AddString(hWndParity, L"偶");
        ComboBox_SetCurSel(hWndParity, (int)(cfg.Parity));

        auto hWndFlowControl = GetDlgItem(hDlg, IDC_COMBO_FLOW_CONTROL);
        ComboBox_AddString(hWndFlowControl, L"无");
        ComboBox_AddString(hWndFlowControl, L"软件(Xon/Xoff)");
        ComboBox_AddString(hWndFlowControl, L"硬件");
        ComboBox_SetCurSel(hWndFlowControl, (int)(cfg.FlowControl));

        auto hWndEncoding = GetDlgItem(hDlg, IDC_COMBO_ENCODING); 
        ComboBox_AddString(hWndEncoding, L"UTF-8 (65001)");
        ComboBox_AddString(hWndEncoding, L"GBK (936)");
        ComboBox_AddString(hWndEncoding, L"BIG5 (950)");
        ComboBox_AddString(hWndEncoding, L"Shift-JIS (932)");

        int encodingIndex = 0;
        switch (cfg.Encoding) {
        case 936: encodingIndex = 1; break;
        case 950: encodingIndex = 2; break;
        case 932: encodingIndex = 3; break;
        default: encodingIndex = 0; break;
        }
        ComboBox_SetCurSel(hWndEncoding, encodingIndex);

        return (INT_PTR)TRUE;
    }
    case WM_DEVICECHANGE:
        UpdatePortControl(hDlg);
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            if (LOWORD(wParam) == IDOK)
            {
                SERIAL_CONFIG cfg = {0};
                auto hWndPort = GetDlgItem(hDlg, IDC_COMBO_PORT);
                auto hWndBaudRate = GetDlgItem(hDlg, IDC_COMBO_SPEED);
                auto hWndWordLength = GetDlgItem(hDlg, IDC_COMBO_WORD);
                auto hWndStopBit = GetDlgItem(hDlg, IDC_COMBO_STOP);
                auto hWndParity = GetDlgItem(hDlg, IDC_COMBO_PARITY);
                auto hWndFlowControl = GetDlgItem(hDlg, IDC_COMBO_FLOW_CONTROL);

                WCHAR txtBuffer[32] = {0};
                auto curSel = ComboBox_GetCurSel(hWndPort);
                if (curSel >= 0)
                {
                    cfg.Serial = (DWORD)ComboBox_GetItemData(hWndPort, curSel);
                }
                else
                {
                    ComboBox_GetText(hWndPort, txtBuffer, 32);
                    cfg.Serial = std::wcstoul(txtBuffer + 3, nullptr, 10);
                }
                ComboBox_GetText(hWndBaudRate, txtBuffer, 32);
                cfg.BaudRate = std::wcstoul(txtBuffer, nullptr, 10);
                cfg.WordLength = ComboBox_GetCurSel(hWndWordLength) + 4;
                cfg.StopBit = ComboBox_GetCurSel(hWndStopBit);
                cfg.Parity = ComboBox_GetCurSel(hWndParity);
                cfg.FlowControl = ComboBox_GetCurSel(hWndFlowControl);

                auto hWndEncoding = GetDlgItem(hDlg, IDC_COMBO_ENCODING);
                int encodingIndex = ComboBox_GetCurSel(hWndEncoding);
                switch (encodingIndex) {
                case 0: cfg.Encoding = CP_UTF8; break;    // 65001
                case 1: cfg.Encoding = 936; break;        // GB2312
                case 2: cfg.Encoding = 950; break;        // BIG5
                case 3: cfg.Encoding = 932; break;        // Shift-JIS
                default: cfg.Encoding = CP_UTF8; break;
                }

                WriteSerialConfig(cfg);
            }
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
