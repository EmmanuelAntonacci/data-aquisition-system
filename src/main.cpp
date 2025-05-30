#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <ctime>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>

using boost::asio::ip::tcp;

#pragma pack(push, 1)
struct LogRecord {
    char sensor_id[32]; // supondo um ID de sensor de até 32 caracteres
    std::time_t timestamp; // timestamp UNIX
    double value; // valor da leitura
};
#pragma pack(pop)

std::time_t string_to_time_t(const std::string& time_string) {
    std::tm tm = {};
    std::istringstream ss(time_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
}

std::string time_t_to_string(std::time_t time) {
    std::tm* tm = std::localtime(&time);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}



class Session : public std::enable_shared_from_this<Session>{
    public:
        explicit Session(tcp::socket socket) : session_socket(std::move(socket)) {}

        void start(){
            receive_message();
        }

    private:
        tcp::socket session_socket;
        std::array<char, 1024> message_buffer;

        std::vector<std::string> handle_split_message(const std::string& s, char delimiter) {
            std::vector<std::string> tokens;
            std::string token;
            std::stringstream ss(s);
            while (std::getline(ss, token, delimiter)) {
                tokens.push_back(token);
            }
            return tokens;
        }

        void receive_message(){
            auto self(shared_from_this());
            session_socket.async_read_some(
            boost::asio::buffer(message_buffer),
            [this, self](boost::system::error_code error_code, std::size_t length) {
                if (!error_code) {
                    std::string message(message_buffer.data(), length);
                    handle_message(message);
                    receive_message();
                } else {
                    std::cerr << "Connection error: " << error_code.message() << std::endl;
                }
            });
        }

        void handle_message(const std::string message){
            std::cout << "Mensagem recebida: " << message << std::endl;
            
            std::vector<std::string> split_message = handle_split_message(message, '|');
            std::string msg_type = split_message[0];
            
            if(msg_type == "LOG"){
                char sensor_id[32];
                std::memset(sensor_id, 0, sizeof(sensor_id));
                std::strncpy(sensor_id, split_message[1].c_str(), sizeof(sensor_id) - 1);
                std::time_t timestamp = string_to_time_t(split_message[2]);
                double value = std::stod(split_message[3]);

                handle_save_log(sensor_id, timestamp, value);
            }
        }

        void handle_save_log(char sensor_id[32], std::time_t timestamp, double value){
            LogRecord log;

            std::memset(log.sensor_id, 0, sizeof(log.sensor_id));
            std::strncpy(log.sensor_id, sensor_id, sizeof(log.sensor_id) - 1);
            log.timestamp = timestamp;
            log.value = value;

            std::string sensor_id_str(sensor_id);

            std::ofstream sensor_file;
            sensor_file.open("./sensor_files/"+sensor_id_str+".bin", std::ios::out | std::ios::binary);

            if(!sensor_file.is_open()){
                std::cerr << "Erro ao abrir file: " << sensor_id_str << std::endl;
            }

            sensor_file.write(reinterpret_cast<const char*>(&log), sizeof(LogRecord));
            
            std::cout << "LogRecord created:" << std::endl;
            std::cout << "Sensor ID: " << log.sensor_id << std::endl;
            std::cout << "Timestamp: " << log.timestamp << std::endl;
            std::cout << "Value: " << log.value << std::endl;

            sensor_file.close();
        }

};


class Server{
    public:
        Server(boost::asio::io_context& io_context, int port) : server_acceptor(io_context, tcp::endpoint(tcp::v4(), port)){
            accept_connections();
        }

    private:
        tcp::acceptor server_acceptor;

        void accept_connections(){
            server_acceptor.async_accept(
                [this](boost::system::error_code error_code, tcp::socket socket) {
                    if (!error_code) {
                        std::cout << "Nova coneção: " << socket.remote_endpoint() << std::endl;
                        std::make_shared<Session>(std::move(socket))->start();
                    }
                    else {
                        std::cerr << "Erro em accept_connections(): " << error_code.message() << std::endl;
                    }

                    accept_connections();
                });
        }
};


int main(int argc, char* argv[]) {
    try{
        boost::asio::io_context io_context;
        Server server(io_context, 9000);
        io_context.run();

    }
    catch (const std::exception& e){
        std::cerr << "Erro em main(): " << e.what() << std::endl;
    }


    return 0;
}