#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>

#include "send_packet.h"


int nrFiles = 0;
int count = 0;
int window = 7;

struct ImageInfo {
  int seqNr;
  int nameLen;
  char *data;
  char fileName[];
}*info[0];

struct sockaddr_in serveraddress;
struct timeval timeout;
int ackNr = 0;

int createSocket(int portnr, char *addr) {
  int sock;

  timeout.tv_sec = 2;
  timeout.tv_usec = 0;

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
    printf("could not create socket\n");
    exit(1);
  }

  memset(&serveraddress, '\0', sizeof(serveraddress));

  serveraddress.sin_port = htons(portnr);
  serveraddress.sin_addr.s_addr = atoi(addr);
  serveraddress.sin_family = AF_INET;
  setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
  return sock;
}


void sendFile(int sock) {
  char *buffer;
  char *image;
  char *ack;
  int totalLen;
  int flag = 1;
  char flagarr[4];
  sprintf(flagarr, "%d", flag);

  int n, size, i;


  size = sizeof(serveraddress);

  buffer = calloc(400, sizeof(char*));
  char seqNr[4];
  char nameLen[4];
  char totalLenchar[4];
  char ackNrchar[4];
  char empty[4];
  char num[4];

  char *fileName = info[count] -> fileName;
  sprintf(seqNr, "%d", info[count] -> seqNr);
  sprintf(nameLen, "%d", info[count] -> nameLen);
  strcat(seqNr, "\n");
  strcat(nameLen, "\n");
  strcat(fileName, "\n");

  totalLen = sizeof(int) + sizeof(seqNr) + sizeof(int) + sizeof(flag) + sizeof(unsigned char) + sizeof(nameLen) + sizeof(fileName) + strlen(info[count] -> data);
  sprintf(ackNrchar, "%d", ackNr);
  sprintf(empty, "%d", 0x7f);
  sprintf(num, "%d", 100);
  sprintf(totalLenchar, "%d", totalLen);

  strcat(buffer, totalLenchar);
  strcat(buffer, "\n");
  strcat(buffer, seqNr);
  strcat(buffer, ackNrchar);
  strcat(buffer, "\n");
  strcat(buffer, flagarr);
  strcat(buffer, "\n");
  strcat(buffer, empty);
  strcat(buffer, "\n");
  strcat(buffer, num);
  strcat(buffer, "\n");
  strcat(buffer, nameLen);
  strcat(buffer, info[count] -> fileName);
  strcat(buffer, info[count] -> data);

  sleep(0.1);
  n = send_packet(sock, (const char *) buffer, (1024 + sizeof(buffer)), 0, (const struct sockaddr *) &serveraddress, sizeof(serveraddress));

  free(buffer);
  ack = calloc(3, sizeof(char));

  if (count < window) {
    count++;
  }
  else {
    if (count < (ackNr + window)) {
      count++;
    }
    else {
      n = recvfrom(sock, (char*)ack, 100, 0, (struct sockaddr *) &serveraddress, &size);

      if (n == -1) {
        count = ackNr;
      }
      else{
        free(info[ackNr]);
        count++;
        ackNr++;
      }
    }
  }
  if (ackNr >= (nrFiles - window)) {
    totalLen = sizeof(int) + sizeof(seqNr) + sizeof(int) + sizeof(flag) + sizeof(unsigned char);
    sprintf(totalLenchar, "%d", totalLen);
    buffer = totalLenchar;
    strcat(buffer, "\n");
    sprintf(seqNr, "%d", count);
    strcat(buffer, seqNr);
    strcat(buffer, "\n");
    strcat(buffer, ackNrchar);
    strcat(buffer, "\n");
    flag = 8;
    sprintf(flagarr, "%d", flag);
    strcat(buffer, flagarr);
    strcat(buffer, "\0");
    n = send_packet(sock, (const char *) buffer, (1024 + sizeof(buffer)), 0, (const struct sockaddr *) &serveraddress, sizeof(serveraddress));

    while (ackNr < nrFiles) {
      n = recvfrom(sock, (char*)ack, 100, 0, (struct sockaddr *) &serveraddress, &size);
      if (n == -1) {
        break;
      }
      else {

        free(info[ackNr]);
        ackNr++;
      }
    }
  }
  free(ack);
}

void readPicture(char *imgName, int sock) {
  FILE *picture;
  char *image;
  char *dir;
  char str[50];

  DIR *directory;
  struct dirent *dirent;
  directory = opendir("./big_set/");

  while ((dirent = readdir(directory)) != NULL) {
      if (!strcmp(dirent -> d_name, "..")|| !strcmp(dirent -> d_name, ".")) {} //to get out the .. and . directory
      else {
        if (strstr(imgName, dirent -> d_name)) {
          strcpy(info[count] -> fileName, dirent -> d_name);
          info[count] -> nameLen = strlen(imgName);

          strcpy(str, "./big_set/");
          dir = strcat(str, dirent -> d_name);
          picture = fopen(dir, "r");

          image = calloc(350, sizeof(char*));
          fread(image, sizeof(char), 1, picture);

          int i = 0;
          while (!feof(picture)) {
            fread(&image[++i], sizeof(char), 1, picture);
          }
          fclose(picture);
          info[count] -> data = image;
          info[count] -> seqNr = count;
          sendFile(sock);
          free(image);
          break;

        }
      }
  }
  closedir(directory);
}

void readFile(char *list, int sock) {
  FILE *nameFile;
  char *imgName;


  nameFile = fopen(list, "r");
  if (nameFile == NULL) {
    printf("Could not open file: %s\n", list);
    exit(1);
  }

  while(1) {
    if(feof(nameFile)) {
      break;
    }
    info[nrFiles] = (struct ImageInfo*) malloc(sizeof(struct ImageInfo) + 60);
    fgets(imgName, 255, nameFile);
    imgName[strlen(imgName)-1] = '\0';
    strcpy(info[nrFiles] -> fileName, imgName);
    nrFiles++;
  }
  nrFiles--;
  while (count <= nrFiles - 1) {
    readPicture(info[count] -> fileName, sock);
  }
  fclose(nameFile);
}

int main(int argc, char** argv) {
  char *addr = argv[1]; //INADDR_ANY
  int portnr = atoi(argv[2]); //8080
  char *fileName = argv[3];
  float prob = atof(argv[4]); //0.2f
  int sock;
  set_loss_probability(prob);

  sock = createSocket(portnr, addr);
  readFile(fileName, sock);
  free(info[nrFiles]);
  close(sock);

  return 0;

}
