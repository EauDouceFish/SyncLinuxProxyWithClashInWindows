#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <regex>
#include <stdexcept>
#include <array>
#include <memory>

/*
    备忘：
    port是自动获取，主机clash配置文件自动修改
    Linux 配置文件存储在 nano ~/.bashrc
    需要手动获取填写虚拟机静态网络环境配置 ip addr show
    需要手动填写clash config路径，虚拟机用户名
*/

const std::string PATH_TO_CLASH_CONFIG_YAML = "C:/Users/25087/.config/clash/config.yaml";
const std::string VM_IP_ADDR = "192.168.1.7";
const std::string VM_USERNAME = "cs180";

// 传入cmd命令，基于管道通信，之后返回执行输出结果
// pipe管道类，用于在进程间通讯，接收一个长度为2的数组作为输入
// 该数组中 0为读取端， 1为写入端
// popen() 函数用于打开一个进程，并创建一个管道以供父进程与子进程间通信
//_popen, _pclose用于Windows环境
std::string execute(const char* cmd) {
    std::array<char, 256> buffer;
    std::string res;

    // 在后台创建一个进程执行cmd，并读取输出
    // _popen会返回FILE*指针，离开作用域后调用_pclose关闭管道
    std::shared_ptr<FILE> pipe(_popen(cmd, "r"), _pclose);

    if (!pipe) throw std::runtime_error("Execute popen() failed!");
   
    // fgets逐行读取命令的输出，之后存放进入buffer
    // 3 paras for fgets: char *str, int num（字符数组大小）, FILE *stream
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        res += buffer.data();
    }
    return res;
}

// 获取主机的WIFI IPv4地址的函数
std::string getWIFIIPv4Address() {
    std::string ipconfigOutput = execute("ipconfig");
    // 更新用正则表达式，确保正确匹配WLAN - IPv4区块下的地址（中英文共通）
    std::regex ipPattern(R"(WLAN:[\s\S]*?IPv4[\s\S]*?([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}))");
    std::smatch ipMatched;

    if (std::regex_search(ipconfigOutput, ipMatched, ipPattern)) {
        return ipMatched[1]; // 返回匹配的IPv4地址，[1]代表返回第一个括号内的值，[0]代表返回整个字符串
    }
    else {
        std::cerr << "WIFI IPv4 match error: Cannot match target by using regular expression." << std::endl;
    }
    return "";
}

// 读取端口信息
int readPortFromConfig(const std::string& configPath) {
    std::ifstream configFile(configPath);
    std::string line;
    int port = -1;

    if (configFile.is_open()) {
        std::regex portPattern(R"(mixed-port:\s*(\d+))");  // 匹配 mixed-port 后面的端口号
        while (std::getline(configFile, line)) {
            std::smatch match;
            if (std::regex_search(line, match, portPattern) && match.size() > 1) {
                port = std::stoi(match[1].str());
                break;
            }
        }
        configFile.close();
    }
    return port;
}


// 更新Clash配置文件
void updateClashConfig(const std::string& ip_address) {
    std::string clashConfigYamlPath = PATH_TO_CLASH_CONFIG_YAML;
    std::ifstream config_in(clashConfigYamlPath);
    std::ofstream config_out(clashConfigYamlPath + ".temp");
    std::string line;

    // 使用正则表达式匹配external-controller行并捕获IP地址和端口号
    std::regex externalControllerPattern(R"(external-controller:\s*(\d+\.\d+\.\d+\.\d+):(\d+))");
    
    // 每个括号代表匹配的部分：match[0]-ip:port;match[1]-ip; match[2]-port;
    std::smatch matchedPart; 

    while (std::getline(config_in, line)) {
        if (std::regex_search(line, matchedPart, externalControllerPattern)) {
            // 用新的IP地址替换原有IP地址，保留端口号
            config_out << "external-controller: " << ip_address << ":" << matchedPart[2] << "\n";
        }
        else {
            config_out << line << "\n";
        }
    }
    config_in.close();
    config_out.close();
    if (std::remove(clashConfigYamlPath.c_str()) != 0) {
        std::cerr << "Failed to remove original config file." << std::endl;
        return;
    }
    if (std::rename((clashConfigYamlPath + ".temp").c_str(), clashConfigYamlPath.c_str()) != 0) {
        std::cerr << "Failed to rename temporary config file." << std::endl;
        return;
    }
    else
    {
        std::cout << "Modified config.yaml successfully!" << '\n';
    }
}


// 检查SSH与虚拟机的通信是否可用
bool testSSHConnection(const std::string& vmIpAddr, const std::string& vmUsername) {

    //ssh cs180@192.168.1.7 exit
    std::string ssh_test_command = "ssh " + vmUsername + "@" + vmIpAddr + " 'exit'";
    //std::string ssh_test_command = "ssh -o BatchMode=yes -o ConnectTimeout=5 " + vmUsername + "@" + vmIpAddr;
    std::string output = execute(ssh_test_command.c_str());

    // Check if the SSH connection test was successful (no output expected)
    if (output.empty()) {
        std::cout << "\033[1;32mSSH connection successful.\033[0m" << std::endl;  // Green text
        return true;
    }
    else {
        std::cerr << "\033[1;31mSSH connection failed: " << output << "\033[0m" << std::endl;  // Red text
        return false;
    }
}


void updateVMProxySettings(const std::string& ipAddr, const std::string& vmIpAddr, const std::string& vmUsername) {
    int port = readPortFromConfig(PATH_TO_CLASH_CONFIG_YAML);
    if (port == -1) {
        std::cerr << "Failed to read port from config file." << std::endl;
        return;
    }

    // Test SSH connection before attempting to update settings
    if (!testSSHConnection(vmIpAddr, vmUsername)) {
        std::cerr << "Cannot establish SSH connection. Aborting proxy update." << std::endl;
        return;
    }

    // 删去所有旧的http/https/all_proxy配置
    //ssh cs180@192.168.1.7 "sed -i '/^export http_proxy=/d' ~/.bashrc &&
    //  sed -i '/^export https_proxy=/d' ~/.bashrc && 
    // sed -i '/^export all_proxy=/d' ~/.bashrc" 

    std::string delOldConfig = "ssh " + vmUsername + "@" + vmIpAddr +
        " \"sed -i '/^export http_proxy=/d' ~/.bashrc && "
        "sed -i '/^export https_proxy=/d' ~/.bashrc && "
        "sed -i '/^export all_proxy=/d' ~/.bashrc\"";

    std::string addHttpProxy = "ssh " + vmUsername + "@" + vmIpAddr +
        " \"echo \\\"export http_proxy=http://" + ipAddr + ":" + std::to_string(port) + "\\\" >> ~/.bashrc\"";

    std::string addHttpsProxy = "ssh " + vmUsername + "@" + vmIpAddr +
        " \"echo \\\"export https_proxy=http://" + ipAddr + ":" + std::to_string(port) + "\\\" >> ~/.bashrc\"";

    std::string addAllProxy = "ssh " + vmUsername + "@" + vmIpAddr +
        " \"echo \\\"export all_proxy=socks5://" + ipAddr + ":" + std::to_string(port) + "\\\" >> ~/.bashrc\"";

    // Execute commands to update proxy settings
    std::string output_delOldConfig = execute(delOldConfig.c_str());
    std::string output_addHttpProxy = execute(addHttpProxy.c_str());
    std::string output_addHttpsProxy = execute(addHttpsProxy.c_str());
    std::string output_addAllProxy = execute(addAllProxy.c_str());

#pragma region Output Info

    //ssh cs180@192.168.1.7 "sed -i '/^export http_proxy=/d' ~/.bashrc
    //  && sed -i '/^export https_proxy=/d' ~/.bashrc && sed -i '/^export all_proxy=/d' ~/.bashrc"

    //ssh cs180@192.168.1.7 "echo \"export http_proxy=http://192.168.1.6:7890\" >> ~/.bashrc"

    std::cout << delOldConfig << " \033[1;32mExecuted Successfully\033[0m" << std::endl;
    std::cout << output_delOldConfig << std::endl;
    std::cout << addHttpProxy << " \033[1;32mExecuted Successfully\033[0m" << std::endl;
    std::cout << output_addHttpProxy << std::endl;
    std::cout << addHttpsProxy << " \033[1;32mExecuted Successfully\033[0m" << std::endl;
    std::cout << output_addHttpsProxy << std::endl;
    std::cout << addAllProxy << " \033[1;32mExecuted Successfully\033[0m" << std::endl;
    std::cout << output_addAllProxy << std::endl;

#pragma endregion Output Info

}

int main() {
    std::string wifi_ip = getWIFIIPv4Address();
    if (!wifi_ip.empty()) {
        std::cout << "Your WIFI IPv4 address FETCHED is: " << wifi_ip << '\n';
        updateClashConfig(wifi_ip);
        std::string vmIpAddr = VM_IP_ADDR;  // 虚拟机的IP地址
        std::string vmUsername = VM_USERNAME;  // 虚拟机的用户名
        updateVMProxySettings(wifi_ip, vmIpAddr, vmUsername);
        std::cout << "VPN Shared, IP Addr: " << wifi_ip << std::endl;
    }
    else {
        std::cerr << "Cannot Fetch WIFI IPv4 Addr!" << std::endl;
    }
    return 0;
}
