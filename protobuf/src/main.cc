#include <iostream>
#include <wait.h>

#include "proto/request.pb.h"
#include "utils.h"
using namespace std;

int main(int argc, char **argv) {
  printf("Main starting with pid %d\n", getpid());
  
  // create pipe to connect the parent and the child process.
  int fds[2];
  if (pipe(fds) < 0) {
    fprintf(stderr, "Failed to create a channel\n");
    std::terminate();
  }

  // fork the process.
  if (int pid = fork(); pid < 0) {
    fprintf(stderr, "Failed to fork a new process\n");
    std::terminate();
  } else if (pid == 0) {
    printf("I'm child process with pid %d\n", getpid());

    core::Request receive;

    // close the write_end and read from read_end.
    close(fds[1]);
    receive.ParseFromFileDescriptor(fds[0]);
    printf("Receive request from parent: %s\n", receive.msg().c_str());
    close(fds[0]);
  } else {
    printf("I'm parent process with pid %d\n", getpid());
    printf("I'm going to send hello request to my child\n");

    core::Request request = create_request("hello", 1024);

    // close the read_end and write to read_end.
    close(fds[0]);
    request.SerializeToFileDescriptor(fds[1]);
    close(fds[1]);

    // wait for the child process to exit.
    int status;
    wait(&status);
    printf("The child process exited\n");
  }

  return 0;
}