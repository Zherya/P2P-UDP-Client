#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Функция использует функцию select() для проверки готовности сокета на чтение или
 * наступления тайм-аута раньше этого события.
 * Функция возвращает -1 в случае тайм-аута ожидания сообщения, -2 - в случае ошибки
 * или число полученных байт в случае успеха: */
ssize_t timeoutRecv(int socketFD, void *buffer, size_t size, struct timeval *timeout) {
    /* Так как нумерация дескрипторов начинается с нуля, то число дескрипторов
     * до последнего проверяемого функцией select() дескриптора
     * (в данном случае одного socketFD) есть величина socketFD + 1: */
    int FDcount = socketFD + 1;
    fd_set FDforRead; /* Набор дескрипторов, проверяемых на доступность чтения */
    /* Сбросим все биты в наборе: */
    FD_ZERO(&FDforRead);
    /* Установим в наборе бит проверяемого дескриптора: */
    FD_SET(socketFD,&FDforRead);
    int returnValue = select(FDcount, &FDforRead, NULL, NULL, timeout);
    /* Если возник тайм-аут или ошибка, то завершаем работу: */
    if (returnValue == 0 || returnValue == -1)
        /* Вычитаем дополнительно единицу, чтобы была возможность различить тайм-аут
         * и сообщение с нулевой длиной (что допустимо для UDP-сокетов): */
        return returnValue - 1;
    /* Иначе дескриптор доступен на чтение (проверять это нет смысла, так как дескриптор один): */
    if ((returnValue = recv(socketFD, buffer, size, 0)) == -1)
        /* Случай ошибки: */
        return - 2;
    else
        return returnValue;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Использование: %s <IPv4-адрес сервера> <UDP-порт сервера>\n", argv[0]);
        return 0;
    }

    /* Заполним структуру адреса сервера, с которым необходимо вести обмен пакетами: */
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    /* Переведем IPv4-адрес из текстового формата в числовой и запишем в структуру адреса сервера: */
    if (inet_pton(AF_INET, argv[1], &serverAddress.sin_addr) != 1) {
        printf("Ошибка конвертации IPv4-адреса в числовой формат\n");
        return 0;
    }
    /* Переведем номер UDP-порта в числовой формат и запишем в структуру адреса сервера: */
    unsigned long uintServerPortValue = strtoul(argv[2], NULL, 0);
    if (uintServerPortValue > 65535) {
        printf("Значение номера UDP-порта не может превышать значение 65535\n");
        return 0;
    }
    if (uintServerPortValue < 49152)
        printf("Не рекомендуется использовать заранее известные или зарегистрированные порты\n");
    in_port_t serverUDPPort = uintServerPortValue;
    serverAddress.sin_port = htons(serverUDPPort);

    /* Создадим UDP-сокет для соединиения с сервером: */
    int clientSockFD = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSockFD == -1) {
        perror("Ошибка создания UDP-сокета для соединения с сервером");
        return 0;
    }
    /* С помощью функции connect() сделаем UDP-сокет присоединенным (настроенным на обмен пакетами только с
     * указанным сервером, по сути), что позволит не указывать каждый раз при чтении и/или записи из/в сокет(а)
     * адрес сервера, а также укажет ядру на то, что входящие UDP-пакеты не от сервера необходимо не передавать
     * на данный сокет. По сути, мы организовываем соединение "точка-точка", что нам и нужно. Более того,
     * присоединенный сокет может получать ассинхронные ошибки: например, если после вызова функции send()
     * (которая завершается до фактической отправки пакета) была получен ICMP-пакет с информацией о
     * недоступности порта, то в случае не присоединенного сокета эту ошибку не вернут сокету и процесс
     * может навсегда зависнуть в ожидании пакета, если после send() была вызвана recv(). В случае
     * присоединенного сокета вызов recv() вернет эту ошибку. */
    if (connect(clientSockFD, (const struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1) {
        perror("Ошибка присоединения UDP-сокета к адресу сервера");
    }

    char message[] = "Hey, server, it's client\n";
    printf("Отправка сообщения серверу...\n");
    ssize_t bytesSendRecv = send(clientSockFD, message, strlen(message) + 1, 0);
    if (bytesSendRecv == -1) {
        perror("Ошибка отправки сообщения серверу");
        return 0;
    } else
        printf("Сообщение отправлено\n");
    if (bytesSendRecv != strlen(message) + 1)
        printf("Отправлено %zd байт вместо %zu байт\n", bytesSendRecv, strlen(message) + 1);

    /* Зададим время тайм-аута для получения пакета от сервера: */
    struct timeval timeout;
    timeout.tv_sec = 10; /* 10 секунд */
    timeout.tv_usec = 0; /* 0 миллисекунд */
    char received[1024] = {};
    printf("Ожидание сообщения от сервера...\n");
    bytesSendRecv = timeoutRecv(clientSockFD, received, 1024, &timeout);
    switch (bytesSendRecv) {
        case -2:
            perror("Ошибка получения сообщения от сервера");
            return 0;
        case -1:
            printf("Тайм-аут ожидания сообщения от сервера\n");
            return 0;
            /* Иначе получение сообщения успешно: */
        default:
            printf("Получено: %s\n", received);
            return 0;
    }
}