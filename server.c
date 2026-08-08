// gcc server.c -o server -pthread
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>

// Максимальное количество клиентов (поставь своё чесло)
#define MAX_CLIENTS 160

#define MAX_BUFFER 1033 // Клиент сможет набрать максимум 1000 символов. 1033: 1000 на сообщение клиента, 32 на ник, 1 на символ конца строки

// Каждый клиент идентифицируется этой структурой
typedef struct {
    int fd; // Файловый дескриптор сокета
    char name[32]; // Ник, чтобы в чате различать, кто и что написал
} Client;

Client clients_list[MAX_CLIENTS]; // Массив всех подключенных client_fd
// При подключении новые клиенты добавляются в конец массива, при отключении последний клиент встает на место того, кто отключился

int client_count = 0; // Счетчик текущих клиентов

// define and initialize a mutex named `clients_list_mutex'
pthread_mutex_t clients_list_mutex = PTHREAD_MUTEX_INITIALIZER;

// Эта функция получает сообщения от клиента и отсылает его всем подключенным клиентам
// Функция создания потока pthread_create запускает в отдельном потоке только функцию, которая принимает аргументы типа void* и возвращает значение типа void* 
void* client_handler(void* arg) {
    // Поток должен стать Detaching threads, чтобы основному потоку не приходилось вызывать pthread_join() для очистки ресурсов этого потока
    pthread_detach(pthread_self());

    Client *client = (Client*)arg; // Перевожу тип аргумента из пустотного в тип-структуру идентифицирующую клиента. Здесь client - указатель на переданную в поток структуру клиента
    int client_fd = client->fd;    // Извлекаю файловый дескриптор сокета клиента из переданной в поток структуры
    char buffer[1000]; // Буфер приема. Клиент может набрать не больше 1000 символов с учетом того, что программа прилепит в конец один символ конца строка '/0'
	  char buffer_and_name[MAX_BUFFER]; // Буфер, который будет хранить и ник клиента и его сообщение
    while (1) {
		  // Возвращает количество принятых bytes
		  ssize_t bytes_read_count = recv(client_fd, buffer, sizeof(buffer) - 1, 0); 
		  // recv_buffer без оператора &, так как переменная buffer уже содержит адрес нулевой ячейки массива
		  // последний аргумент - 0 означает, что флаги не требуются
		  if (bytes_read_count == 0){
		  	printf("The client disconnected, recv()\n");
		  	break;
		  }
		  else if (bytes_read_count < 0){
		  	perror("Error: Ошибка чтения полученного сообщения, recv()\n");
		  	break;
		  }
      buffer[bytes_read_count] = '\0';	// Добавляю в конец полученного сообщения символ конца строки
      // Приклею к строке ник отправителя перед рассылкой остальным клиентам чата
      // snprintf вставит символ конца строки
      snprintf(buffer_and_name, sizeof(buffer_and_name), "[%s]: %s", client->name, buffer);

      // Произвожу рассылку сообщения остальным клиентам чата
      // Блокирую доступ к списку клиентов, чтобы основной поток не удалял и не добавлял клиентов, пока рассылка не завершится, на всякий случай проверяю на наличие ошибок
	    if (pthread_mutex_lock(&clients_list_mutex) != 0){
		      fprintf(stderr, "Error: Ошибка pthread_mutex_lock(&clients_list_mutex)\n"); // perror использует errno, но pthread_create её НЕ устанавливает, поэтому fprintf
	    }

      for (int i = 0; i < client_count; i++){
        if (clients_list[i].fd == client_fd) { // Не буду отсылать клиенту его же сообщение
          continue;
        } 
        send(clients_list[i].fd, buffer_and_name, strlen(buffer_and_name), 0);
      }

      // Рассылка окончена. Разблокирую доступ к списку клиентов
      if (pthread_mutex_unlock(&clients_list_mutex) != 0){
		      fprintf(stderr, "Error: Ошибка pthread_mutex_unlock(&clients_list_mutex)"); // perror использует errno, но pthread_create её НЕ устанавливает, поэтому fprintf
	    }
	  }

    // Избавляюсь от отключившегося клиента
    //Удаляю клиента из списка
    	if (pthread_mutex_lock(&clients_list_mutex) != 0){
		      fprintf(stderr, "Error: Ошибка pthread_mutex_lock(&clients_list_mutex)\n"); // perror использует errno, но pthread_create её НЕ устанавливает, поэтому fprintf
	    }

      for (int i = 0; i < client_count; i++){
        if (clients_list[i].fd == client_fd) {
          clients_list[i] = clients_list[client_count-1]; // Заменяю ушедшего клиента последним зашедшим клиентом
          client_count--;
          break;
        } 
      }

      // Рассылка окончена. Разблокирую доступ к списку клиентов
      if (pthread_mutex_unlock(&clients_list_mutex) != 0){
		      fprintf(stderr, "Error: Ошибка pthread_mutex_unlock(&clients_list_mutex)\n"); // perror использует errno, но pthread_create её НЕ устанавливает, поэтому fprintf
	    }

    close(client_fd);
    free(client); 
	  return NULL;
}

int main() {
    // Создаю сокет
    // Socket function returns a file descriptor (sockfd)
    // AF_INET - IPv4 protocol family, SOCK_STREAM - stream socket type, 0 - system's default protocol augment.
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
      perror("Error: Ошибка создания сокета :( socket()\n");
      return -1;
    }
    printf("Сокет создан успешно. Номер сокета: %d\n", server_fd);

    // Создаю структуру - адрес сокета. (см. содержимое  структуры sockaddr_in в man)
    struct sockaddr_in server_addr;
    // sockaddr_in - IPv4 Socket Address Structure, defined by including the <netinet/in.h> header.
    // Очищаю память структуры адреса сокета. Параметры: указатель на начало структуры, хранит адрес, 0 - все байты структуры станут равны нулю, sizeof вычисляет размер структуры в байтах
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET; //IPv4 protocol family
    server_addr.sin_port = htons(8080); //htons меняет порядок байтов хоста на Big-Endian (сетевой формат)
    // INADDR_ANY равна 0.0.0.0. When listen(2) is called on an unbound socket, the socket is automatically bound to a random free port with the local address set to INADDR_ANY.
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); //htonl переводит long в Big-Endian, но для 0.0.0.0 это необязатьно
    int result_of_call = bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (result_of_call < 0){
      perror("Error: Ошибка назначения локального протокольного адреса сокету, bind()\n");
      close(server_fd); // Закрываю сокет
      return -1;
    }
    printf("Локальный протокольный адрес сокета назначен успешно, bind()\n");
    
    // listen переводит unconnected сокет в пассивный режим и обещает, что ядро примет входящее соединение, направленное на этот сокет
    // размер очереди входящих подключений - 16
    result_of_call = listen(server_fd, 16);
    if (result_of_call < 0){
      perror("Error: Ошибка перевода сокетв в пассивный режим, listen()\n");
      close(server_fd); // Закрываю сокет
      return -1;
    }
    printf("Сокет переведен в пассивный режим успешно, listen()\n");
    
    // Адрес сокета клиента. Функция accept(), при подключении клиета, запишет IPv4 адрес и порт в эту структуру
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr); // Объявлена отдельная переменная так как accept принимает указатель переменную типа socklen_t. Она нужна, чтобы ядро не вышло за пределы памяти при записи размера полученного адреса клиента

    
    // Реализую многопоточность, чтобы сервер мог обслуживать сразу несколько клиентов.
    // Основной поток прожигает время в ожидании и принятии подключения от новых клиентов - accept(). После того как новое подключение accept, основной поток создает новый поток и передает ему клиентский файловый дескриптор сокета.
    while (1) {
      // Блокирующий accept(). Cм. man 2 accept. accept extracts the first connection request on the queue of pending connections for the listening socket, sockfd, creates a new connected socket, and returns a new file descriptor referring to that socket.  The newly created socket is not in the listening state. The original socket sockfd is unaffected by this call.
      // Return a file descriptor for the accepted socket
      int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len); // структура адреса клиента типа sockaddr_in приводится к универсальному типу sockaddr
      if (client_fd < 0){
        perror("Error: Ошибка приема входящего соединения, accept()\n");
        continue; // Перехожу к блокирующему accept()
      }

      // Клиентов не может быть больше MAX_CLIENTS, чтобы не выйти за границы массива
      // Блокирую список клиентов для чтения (на случай, если второй поток в это время будет удалять из него клиента)
      if (pthread_mutex_lock(&clients_list_mutex) != 0){
		      fprintf(stderr, "Error: Ошибка pthread_mutex_lock(&clients_list_mutex)\n"); // perror использует errno, но pthread_create её НЕ устанавливает, поэтому fprintf
	    }
      if (client_count >= MAX_CLIENTS) {
        send(client_fd, "Server full", 11, 0); // Сообщаю клиенту, что сервер переполнен
        close(client_fd); // Закрываю сокет отвергнутого клиента. Клиент поймет, что связь разорвана, так как в его коде есть условие, проверяющее recv, возвращающее 0 - получение пакета FIN
        if (pthread_mutex_unlock(&clients_list_mutex) != 0){
		      fprintf(stderr, "Error: Ошибка pthread_mutex_unlock(&clients_list_mutex)\n"); // perror использует errno, но pthread_create её НЕ устанавливает, поэтому fprintf
	      }
        continue; // Перехожу к блокирующему accept()
      }
      // Разблокирую список клиентов
      if (pthread_mutex_unlock(&clients_list_mutex) != 0){
		      fprintf(stderr, "Error: Ошибка pthread_mutex_unlock(&clients_list_mutex)\n"); // perror использует errno, но pthread_create её НЕ устанавливает, поэтому fprintf
	    }

      // Извлекаю IP-адрес и порт клиента из структуры - адреса сокета и выводят в терминал
      char client_ip[INET_ADDRSTRLEN]; // INET_ADDRSTRLEN = 16 байтов
      int client_port = ntohs(client_addr.sin_port); // network to host short
      if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)) == NULL){
          perror("Ошибка перевода IP-адреса клиента из сетевого формата в строковый\n");
      };

      printf("Входящее соединение принято успешно, accept(). Получен клиентский файловый дескриптор сокета: %d. IP-адрес клиента: %s, порт: %d. Количество подключенных клиентов: %d\n", client_fd, client_ip, client_port, client_count + 1);

      char nick[32];
      // Возвращает количество принятых bytes
		  ssize_t bytes_read_count = recv(client_fd, nick, sizeof(nick) - 1, 0); 
      if (bytes_read_count == 0){
			printf("Ошибка: Ник пуст, recv(). Anonymous разорвал соединение\n");
       close(client_fd); // закрываю сокет
       continue; // Перехожу к блокирующему accept()
		}
		else if (bytes_read_count < 0){
			perror("Error: Ошибка чтения ника, recv(). Теперь его зовут Anonymous.\n");
      strcpy(nick, "Anonymous");
		}
    else {
      nick[bytes_read_count] = '\0';
    }

      // Блокирую список клиентов, чтобы добавить нового клиента
      if (pthread_mutex_lock(&clients_list_mutex) != 0){
		      fprintf(stderr, "Error: Ошибка pthread_mutex_lock(&clients_list_mutex)\n"); // perror использует errno, но pthread_create её НЕ устанавливает, поэтому fprintf
	    }

      // Выделяю память для нового клиента динамически, чтобы передать его потоку
      Client *new_client = malloc(sizeof(Client));
      new_client->fd = client_fd;
      strcpy(new_client->name, nick);
      // Копирую клиента в список клиентов для того, чтобы был список подключенных клиентов
      clients_list[client_count] = *new_client;
      client_count++;

      // Разблокирую список клиентов
      if (pthread_mutex_unlock(&clients_list_mutex) != 0){
		      fprintf(stderr, "Error: Ошибка pthread_mutex_unlock(&clients_list_mutex)\n"); // perror использует errno, но pthread_create её НЕ устанавливает, поэтому fprintf
	    }
 
	    // Создаю новый поток
	    pthread_t thread_id; // TID нового потока определяется с помощью аргумента thread при успешном вызове pthread_create().
	    result_of_call = pthread_create (&thread_id, // Сюда запишется ID созданного потока
						NULL, // Атрибуты потока по умолчанию
						client_handler, // Функция receive_handler, которую будет выполнять поток
						new_client); // Аргументы для функции receive_handler - динамически выделенная структура
            if (result_of_call != 0){
		      fprintf(stderr, "Error: Ошибка создания потока: %s\n", strerror(result_of_call)); // perror использует errno, но pthread_create её НЕ устанавливает, поэтому fprintf
	    }
	    printf("Поток для приема сообщения успешно создан. TID: %lu\n", (unsigned long)thread_id); // Перевожу ID потока из типа pthread_t в тип unsigned long
    }
}
