module.exports = {
  createServer: () => {
    const onClient = () => {
      return "HTTP/1.1 200 OK\n" +
      "Content-Length: 40\n" +
      "Connection: close\n" +
      "Content-Type: text/html\n" +
      "\n"+ 
      "Hello world from userland!";
    }
    return {
      listen(port, hostname, onConnection) {
        return _listen([port, hostname, onClient]) && onConnection();
      }
    };
  }
}