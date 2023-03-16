# HTTP-Server-and-CGI-Programs
實作concurrent http server和cgi，使用戶得以透過瀏覽器直接access遠端工作站，遠端工作站內容可以參考我的另外一個repo：https://github.com/luckyjp6/Remote-Working-Ground-Server。
- 支援於Linux或Windows系統的機器上架設http server。
- 目前實作為了展示server concurrent process request的效果，包了一層panel.cgi，
  用於協助選取連線的remote working ground server和測資，remote working server預設在交大Linux工作站上，
  可自行更改panel.cgi內容調整希望連線的遠端工作站，可使用repo提供的single_proc_shell作為工作站執行檔。
- 使用Boost.Asio實作，請確認您的電腦是否有此函式庫

## For Linux
### Usage
  產生執行檔：```make Linux```  
  移除：```make clean_Linux```  
  開啟server：```./http_server [port]```  
  瀏覽器access服務：```[ip]:[port]/panel.cgi```  
## For Windows  
### Usage  
  產生執行檔：```make Windows```  
  移除：```make clean_Windows```  
  開啟server：```windows_cgi_server.exe [port]```  
  瀏覽器access服務：```[ip]:[port]/panel.cgi```  

## Demo
  The server is on Linux device and the browser is on Windows device.  
  Reloading the page to better show the concurrent part.  
  
https://user-images.githubusercontent.com/96563567/225525631-d3a73821-adf6-4530-847b-163afe545db9.mp4

