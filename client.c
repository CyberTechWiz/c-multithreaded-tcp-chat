// gcc client.c -o client -pthread
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

// Для цветных сообщений в консоли
#define ANSI_COLOR_RED     "\033[31m"
#define ANSI_COLOR_GREEN   "\033[32m"
#define ANSI_COLOR_BLUE    "\033[34m"
#define ANSI_COLOR_CYAN    "\033[36m"
#define ANSI_COLOR_PURPLE  "\033[35m"

#define ANSI_COLOR_PALE    "\033[2m"
#define ANSI_COLOR_ITALIC  "\033[3m"

#define ANSI_PURPLE_ITALIC "\033[3;35m"

#define ANSI_COLOR_RESET   "\033[0m"

// IP-адрес сервера (поставь свой)
#define SERVER_IP "192.168.0.14"

#define MAX_BUFFER 1033 // Клиент сможет набрать максимум 1000 символов. 1033: 1000 на сообщение клиента, 32 на ник, 1 на символ конца строки

// Функция создания потока pthread_create запускает в отдельном потоке только функцию, которая принимает аргументы типа void* и возвращает значение типа void* 
void* receive_handler(void* arg) {
    int client_fd = *(int*)arg; // Перевожу тип аргумента из пустотного в целочисленный и разыменовываю указатель, извлекая нужное значение - сам файловый дескриптор сокета клиента.
    char recv_buffer[MAX_BUFFER]; // Буфер приема. Клиент может набрать не больше 1000 символов с учетом того, что программа прилепит в конец один символ конца строка '/0'
	while (1) {
		// Возвращает количество принятых bytes
		ssize_t bytes_read_count = recv(client_fd, recv_buffer, sizeof(recv_buffer) - 1, 0); 
		// recv_buffer без оператора &, так как переменная recv_buffer уже содержит адрес нулевой ячейки массива
		// последний аргумент - 0 означает, что флаги не требуются
		if (bytes_read_count == 0){
			printf(ANSI_COLOR_RED "The server disconnected, recv()" ANSI_COLOR_RESET "\n");
			break;
		}
		else if (bytes_read_count < 0){
			perror("Error: Ошибка чтения полученного сообщения, recv()\n");
			break;
		}
		recv_buffer[bytes_read_count] = '\0';	// Добавляю в конец полученного сообщения символ конца строки
		printf(ANSI_COLOR_PALE "\n[Получено сообщение]: " ANSI_COLOR_CYAN "%s" ANSI_COLOR_RESET "\n", recv_buffer);
	}
	return NULL;
}

int main() {
    // Создаю сокет
    // Socket function returns a file descriptor (sockfd)
    // AF_INET - IPv4 protocol family, SOCK_STREAM - stream socket type, 0 - system's default protocol augment
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
      perror("Error: Ошибка создания сокета socket()\n");
      return -1;
    }
    printf("Сокет создан успешно. Твой клиентский файловый дескриптор сокета: %d\n", client_fd);

    // Создаю структуру - адрес сокета. (см. содержимое  структуры sockaddr_in в man)
    struct sockaddr_in server_addr;
    // sockaddr_in - IPv4 Socket Address Structure, defined by including the <netinet/in.h> header
    // Очищаю память структуры адреса сокета. Параметры: указатель на начало структуры, хранит адрес, 0 - все байты структуры станут равны нулю, sizeof вычисляет размер структуры в байтах
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; //IPv4 protocol family
    server_addr.sin_port = htons(8080); //htons меняет порядок байтов хоста на Big-Endian (сетевой формат)
    // 192.168.0.14 - адрес интерфейса, на котором запускается серверная часть программы (адрес сервера)
    // SERVER_IP - строковая константа, определена с помощью define
    int result_of_call = inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr); //pton (presentation to network) преобразует IPv4 или IPv6 - адрес из формата строки в бинарное значение в сетевом формате
    if (result_of_call == 0){
      perror("Error: Not a valid presentation format\n");
      close(client_fd); // Закрываю сокет
      return -1;
    }
    else if (result_of_call < 0){
      perror("Error: Error of pton\n");
      close(client_fd); // Закрываю сокет
      return -1;
    }
    printf (ANSI_COLOR_PALE "Преобразование ареса сервера из строкового в сетевой формат выполнено успешно" ANSI_COLOR_RESET "\n");
    
    char name[32];
    printf(ANSI_PURPLE_ITALIC "\nВведите ник, который увидят другие участники чата (максимум 32 байта). Или ничего не вводите, чтобы ник остался Anonymous: " ANSI_COLOR_RESET "\n");
    if (fgets(name, sizeof(name), stdin) != NULL) { // fgets считывает текст до первого символа \n 
      // Меняю символ переноса на символ конца строки. strcspn принимает две строки. В первой строке ищет первый попавшийся символ, который есть во второй строке. Возвращает индекс первого найденного символа
      name[strcspn(name, "\n")] = '\0'; 
      if (strlen(name) == 0) {
          strcpy(name, "Anonymous");
      }
    }

    // Establish a connection with a TCP server
    result_of_call = connect(client_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (result_of_call < 0){
      perror("Error: Ошибка функции установки соединения с сервером connect()\n");
      close(client_fd); // Закрываю сокет
      return -1;
    }
    printf(ANSI_COLOR_GREEN "Соединение с сервером установлено, connect()" ANSI_COLOR_RESET "\n");
    // Передаю серверу ник клиента
    send(client_fd, name, strlen(name), 0);
    printf(ANSI_PURPLE_ITALIC "Привет. Можешь ввести сообщение и нажать enter чтобы отправить его на сервер. Сервер отправит твоё сообщение всем подключенным к нему клиентам! Для выхода из программы введи" ANSI_COLOR_CYAN "!exit" ANSI_COLOR_RESET "\n");

	// Реализую многопоточность, чтоб клиент мог одновременно получать и отправлять сообщения
	// Создаю новый поток
	pthread_t thread_id; // TID нового потока определяется с помощью аргумента thread при успешном вызове pthread_create()
	result_of_call = pthread_create (&thread_id, // Сюда запишется ID созданного потока
						NULL, // Атрибуты потока по умолчанию
						receive_handler, // Функция receive_handler, которую будет выполнять поток
						&client_fd); // Аргументы для функции receive_handler
	if (result_of_call != 0){
		fprintf(stderr, "Error: Ошибка создания потока: %s\n", strerror(result_of_call)); // perror использует errno, но pthread_create её НЕ устанавливает, поэтому fprintf
	}
	printf(ANSI_COLOR_PALE "Поток для приема сообщения успешно создан. Его TID: %lu" ANSI_COLOR_RESET "\n", (unsigned long)thread_id); // Перевожу ID потока из типа pthread_t в тип unsigned long
  
	// Главный поток в бесконечном цикле ожидания ввода пользователя
	char send_buffer[MAX_BUFFER]; // Буфер отправки
	while (1) {
    // fgets читает данные из файлового потока  ввода (stdin) в send_buffer . Возвращает указатель на переданный буфер (send_buffer), см. man 3 fgets. Также функция всегда добавляет символ конца строки '\0', так что читает ровно send_buffer - 1 символов из потока.
    if (fgets(send_buffer, sizeof(send_buffer), stdin) == NULL) {
      if (ferror(stdin)) {
        perror("Ошибка чтения из stdin");
      }
      break;
		}
		// Для красоты можно убрать символ переноса строки '\n', который сохраняет fgets
    // strcspn принимает две строки. В первой строке ищет первый попавшийся символ, который есть во второй строке. Возвращает индекс первого найденного символа
    send_buffer[strcspn(send_buffer, "\n")] = '\0';
		// Проверка на случай, если клиент захочет выйти
    // strcmp сравнивает две строки и возвращает 0, если они равны
    if (strcmp(send_buffer, "!exit") == 0) {
        printf(ANSI_PURPLE_ITALIC "Завершение работы..." ANSI_COLOR_RESET "\n");
        break;
    }
    // Проверка, что строка не пустая
    // strlen возвращает количество символов строки до '/0'
    if (strlen(send_buffer) > 0) {
      send(client_fd, send_buffer, strlen(send_buffer), 0);
    }
}
		
	// Закрываю сокет и завершаю программу
    close(client_fd);
    return 0;
}
