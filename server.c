#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <dirent.h>

#include "pgmread.h"


struct ImageInfo {
  int recieved;
  int seqNr;
  int width;
  int height;
  char *data;
  char fileName[];
}*info[0], *compImage[0];

int i = 0;
int count = 0;
int listenCount = 0;
int acked = -1;
int nrOfImg = 0;

int imageComp(struct ImageInfo *image1, struct ImageInfo *image2) {
  int h, w;
  if (image1 == NULL || image2 == NULL) {
    return 0;
  }
  if (image1 -> width != image2 -> width) {
    return 0;
  }
  if (image1 -> height != image2 -> height) {
    return 0;
  }
  for( h=0; h<image1->height; h++ )
   {
       for( w=0; w<image1->width; w++ )
       {
           if( image1->data[h*image1->width+w] != image2->data[h*image1->width+w] ) {
             return 0;
           }
       }
   }
   free(image2);
  return 1;
}

int populateStruct(char *buffer) {
  int seqNr;
  int nameLen;
  char *fileName;
  char *data;
  char *token;
  int width;
  int height;
  char *temp;
  int totalLen;
  int ackNr;
  unsigned char *flag;
  int flagint;

  token = strtok(buffer, "\n");
  totalLen = atoi(token);
  token = strtok(NULL, "\n");
  seqNr = atoi(token);
  token = strtok(NULL, "\n");
  ackNr = atoi(token);
  flag = strtok(NULL, "\n");
  flagint = atoi(flag);

  if (flagint == 1) {
    if (seqNr == i) {
      token = strtok(NULL, "\n");
      token = strtok(NULL, "\n");
      token = strtok(NULL, "\n");
      strcpy(info[i] -> fileName, token);
      info[i] -> recieved = 1;
      token = strtok(NULL, "\0");
      temp = strtok(token, "\n");
      temp = strtok(NULL, "\n");
      sscanf(temp, "%d %d", &width, &height);
      info[i] -> width = width;
      info[i] -> height = height;
      temp = strtok(NULL, "\n");
      temp = strtok(NULL, "\0");
      info[i] -> data = temp;
      i++;
      return 1;
    }
    else {
      info[i] -> recieved = 0;
      return 0;
    }
  }
  return 2;
}


void receiveFile(int portnr, char *catName, char *uFileName) {
  int sock;
  char *buffer;
  char *ack = "ACK";


  struct sockaddr_in serveraddress;
  struct sockaddr_in clientaddress;
  struct timeval timeout;
  timeout.tv_sec = 7;
  timeout.tv_usec = 0;


  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
      printf("could not create socket\n");
      exit(1);
  }
  setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));


  memset(&serveraddress, '\0', sizeof(serveraddress));
  memset(&clientaddress, '\0', sizeof(clientaddress));

  serveraddress.sin_port = htons(portnr);
  serveraddress.sin_addr.s_addr = INADDR_ANY;
  serveraddress.sin_family    = AF_INET;

  if ( bind(sock, (const struct sockaddr *)&serveraddress, sizeof(serveraddress)) < 0 ) {
      printf("could not bind socket\n");
      exit(1);
  }

  FILE *picture;
  char *image;
  char *dir;
  char str[50];

  DIR *directory;
  struct dirent *dirent;
  directory = opendir(catName);

  int compint = 0;

  while ((dirent = readdir(directory)) != NULL) {
    if (!strcmp(dirent -> d_name, "..")|| !strcmp(dirent -> d_name, ".")) {} //to get out the .. and . directory
    else {
      compImage[compint] = (struct ImageInfo*) malloc(350);
      memset(compImage[compint], '\0', sizeof(compImage[compint]));
      strcpy(compImage[compint] -> fileName, dirent -> d_name);

      strcpy(str, "./big_set/");
      dir = strcat(str, dirent -> d_name);
      picture = fopen(dir, "r");

      image = calloc(1600, sizeof(char*));
      fread(image, sizeof(char), 1, picture);

      int i = 0;
      int height;
      int width;
      char *temp;
      char *temp2;
      while (!feof(picture)) {
        fread(&image[++i], sizeof(char), 1, picture);
      }
      temp = strtok(image, "\n");
      temp = strtok(NULL, "\n");
      sscanf(temp, "%d %d", &width, &height);
      compImage[compint] -> width = width;
      compImage[compint] -> height = height;
      temp2 = strtok(NULL, "\n");
      temp2 = strtok(NULL, "\0");
      fclose(picture);
      compImage[compint] -> data = temp2;
      compImage[compint] -> seqNr = compint;
      compint++;
      nrOfImg++;

      free(image);
    }
  }
  closedir(directory);
  // printf("%d\n", compint);
  FILE *outFile = fopen(uFileName, "w");

  int size, n;
  size = sizeof(clientaddress);  //size is value/resuslt

  buffer = calloc(350, sizeof(char*));
  while (1) {
    if (acked == -1) {
      acked = 0;
    }
    info[i] = (struct ImageInfo*) malloc(350);

    n = recvfrom(sock, (char *)buffer, 2000, 0, ( struct sockaddr *) &clientaddress, &size);
    if (n == -1) {
      break;
    }
    int j;
    j = populateStruct(buffer);
    if (j == 1) {
      if (i < 7) {
        int h;
        int l;
        for(h = 0; h < nrOfImg; h++) {
          if (info[i] == NULL) {
            break;
          }
          l = imageComp(info[i], compImage[h]);
          if (l == 1) {
            fprintf(outFile, "<%s> <%s>\n", info[i] -> fileName, compImage[h] -> fileName);
            // free(compImage[h]);
            break;
          }
        }
        if (l == 0 && info[i] != NULL) {
          fprintf(outFile, "<%s> UNKNOWN\n", info[i] -> fileName);
        }
      }
    }

    if (j == 2) {
      while (acked < i) {
        if (info[acked] -> recieved == 0) {
          sleep(3);
        }
        n = sendto(sock, (char*)ack, strlen(ack), 0, (const struct sockaddr *) &clientaddress, size);
        free(info[acked]);
        acked++;
      }
      printf("avslutter\n");
      free(info[i]);
      fclose(outFile);
      break;
    }

    if (j == 0) {
      free(info[i]);
    }
    if (acked > (i - 7)) {
      //do nothing
    }
    else{
      if (info[acked] -> recieved == 1) {
        int h;
        int l;
        for(h = 0; h < nrOfImg; h++) {
          if (info[i] == NULL) {
            break;
          }
          l = imageComp(info[i], compImage[h]);
          if (l == 1) {
            fprintf(outFile, "<%s> <%s>\n", info[i] -> fileName, compImage[h] -> fileName);
            // free(compImage[h]);
            break;
          }
        }
        if (l == 0 && info[i] != NULL) {
          fprintf(outFile, "<%s> UNKNOWN\n", info[i] -> fileName);
        }
        n = sendto(sock, (char*)ack, strlen(ack), 0, (const struct sockaddr *) &clientaddress, size);
        free(info[acked]);
        acked++;
      }
      else {
        i = acked;
      }
    }
  }
  free(buffer);
  close(sock);
}

int main(int argc, char** argv) {
  int portnr = atoi(argv[1]); //8080
  char *catName = argv[2]; //./big_set/
  char *uFileName = argv[3]; //hei.txt
  receiveFile(portnr, catName, uFileName);


  // I have a problem where when i run the program on valgrind it will run differently than if i run it without.
  //
  // I have tried to check for errors with -Wall and -Wextra and debugged with valgrind with -g and didnt find any
  //errors which had anything to do with this. I really cant find a way to allocate the memory better or
  //free it earlier.

  return 0;
}
