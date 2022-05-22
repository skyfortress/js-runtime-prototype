#include <iostream>
#include <cstdlib>
#include <uv.h>
#include <libplatform/libplatform.h>
#include <v8.h>
#include <unordered_map>
using namespace std;
using namespace v8;

uv_loop_t *loop;

typedef struct {
    uv_write_t req;
    uv_buf_t buf;
} write_req_t;

char* loadScript(string fileName) {
  ifstream t(fileName);
  string script((std::istreambuf_iterator<char>(t)), istreambuf_iterator<char>());

  char* cscript = new char[script.size() + 1];
  strcpy(cscript, script.c_str());
  return cscript;
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = (char*) malloc(suggested_size);
    buf->len = suggested_size;
}

// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}

void free_write_req(uv_write_t *req) {
    write_req_t *wr = (write_req_t*) req;
    free(wr->buf.base);
    free(wr);
}

void echo_write(uv_write_t *req, int status) {
    if (status) {
        fprintf(stderr, "Write error %s\n", uv_strerror(status));
    }
    free_write_req(req);
}

////////////////////////
class Callback {
  private: 
     Global<Context> _context;
     Global<Function> _function;
  public:
    Isolate* _isolate;
    uv_timer_t timer_req;
    uv_tcp_t* server; 
    sockaddr_in* addr;
    Callback(Isolate* isolate,Local<Function> function)
      : _isolate(isolate), _function(isolate, function) {
        _context.Reset(isolate, isolate->GetCurrentContext());
      }

  MaybeLocal<Value> run() {
    HandleScope handle_scope(_isolate);
    Local<Context> context = Local<Context>::New(_isolate, _context);
    Context::Scope context_scope(context);

    Local<Function> function = _function.Get(_isolate);
    return function->Call(context, context->Global(), 0, nullptr);
  }

  void setServerVars() {
    addr = new sockaddr_in();
    server = new uv_tcp_t();
  }
};

unordered_map<void*, Callback*> callbacks;


void echo_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    if (nread > 0) {
        write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
        req->buf = uv_buf_init(buf->base, nread);

        auto cb = callbacks[client];
        String::Utf8Value str(cb->_isolate, cb->run().ToLocalChecked());
        const char* resText = ToCString(str);
              
        cout << resText;
        uv_buf_t res = uv_buf_init((char*)resText, strlen(resText));
        uv_write_t *write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
        uv_write((uv_write_t*)req, client, &res, 1, echo_write);
        return;
    }

    if (nread < 0) {
        if (nread != UV_EOF)
            fprintf(stderr, "Read error %s\n", uv_err_name(nread));
        uv_close((uv_handle_t*) client, NULL);
        //request is ready we can start sending the result
    }
}

void uv_onconnection(uv_stream_t *server, int status) {
  if (status < 0) {
    
      fprintf(stderr, "New connection error %s\n", uv_strerror(status));
      // error!
      return;
  }
  uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
  uv_tcp_init(loop, client);
  callbacks[client] = callbacks[server];
  if (uv_accept(server, (uv_stream_t*) client) == 0) {
    uv_read_start((uv_stream_t*) client, alloc_buffer, echo_read);
  } else {
    uv_close((uv_handle_t*) client, NULL);
  }
};

void timeoutCallback(uv_timer_t* handle) {
  Callback* cb = callbacks[handle];
  cb->run();
  delete cb;
  callbacks.erase(handle);
}

void intervalCallback(uv_timer_t* handle) {
  Callback* cb = callbacks[handle];
  cb->run();
}

void RegisterTimeout(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Local<Object> obj = args[0]->ToObject(context).ToLocalChecked();
  uint64_t timeout = (int64_t)obj->Get(context, 1).ToLocalChecked().As<Number>()->Value();

  Callback* callback = new Callback(isolate,Local<Function>::Cast(obj->Get(context, 0).ToLocalChecked()));
 
  uv_timer_init(loop, &callback->timer_req);
  uv_timer_start(&callback->timer_req, timeoutCallback, timeout, 0);
  callbacks[&callback->timer_req] = callback;
}

void RegisterInterval(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Local<Object> obj = args[0]->ToObject(context).ToLocalChecked();
  uint64_t interval = (int64_t)obj->Get(context, 1).ToLocalChecked().As<Number>()->Value();

  Callback* callback = new Callback(isolate, Local<Function>::Cast(obj->Get(context, 0).ToLocalChecked()));
 
  uv_timer_init(loop, &callback->timer_req);
  uv_timer_start(&callback->timer_req, intervalCallback, interval, interval);
  callbacks[&callback->timer_req] = callback;
}

void ExistsSync(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();

  String::Utf8Value str(args.GetIsolate(), args[0]);
  const char* path = ToCString(str);

  uv_fs_t req;
  
  int res = uv_fs_stat(loop, &req, path, NULL);
  uv_fs_req_cleanup(&req);
  args.GetReturnValue().Set(res);
}

// The callback that is invoked by v8 whenever the JavaScript 'print'
// function is called.  Prints its arguments on stdout separated by
// spaces and ending with a newline.
void Print(const FunctionCallbackInfo<Value>& args) {
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    HandleScope handle_scope(args.GetIsolate());
    if (first) {
      first = false;
    } else {
      printf(" ");
    }
    String::Utf8Value str(args.GetIsolate(), args[i]);
    const char* cstr = ToCString(str);
    printf("%s", cstr);
  }
  printf("\n");
  fflush(stdout);
}

void Require(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  String::Utf8Value str(isolate, args[0]);
  const char* filename = ToCString(str);
  
  Local<Context> context = isolate->GetCurrentContext();
  v8::EscapableHandleScope handle_scope(isolate);

  char* scriptTxt = loadScript(filename);

  Local<String> source = String::NewFromUtf8(isolate, strcat(loadScript(filename), "\n module;"), NewStringType::kNormal).ToLocalChecked();
    // Compile the source code.
  Local<Script> script = Script::Compile(context, source).ToLocalChecked();
    // Run the script to get the result.
  Local<Value> result = script->Run(context).ToLocalChecked();
  args.GetReturnValue().Set(handle_scope.Escape(result));
}

void Listen(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
    Local<Context> context = isolate->GetCurrentContext();
    Local<Object> obj = args[0]->ToObject(context).ToLocalChecked();
    int port = (int)obj->Get(context, 0).ToLocalChecked().As<Number>()->Value();

    String::Utf8Value str(args.GetIsolate(), obj->Get(context, 1).ToLocalChecked());
    const char* host = ToCString(str);

    Callback* clientHandler = new Callback(isolate, Local<Function>::Cast(obj->Get(context, 2).ToLocalChecked()));
    clientHandler->setServerVars();
    callbacks[clientHandler->server] = clientHandler;
    uv_tcp_init(loop, clientHandler->server);

    uv_ip4_addr(host, port, clientHandler->addr);

    uv_tcp_bind(clientHandler->server, (const struct sockaddr*)clientHandler->addr, 0);

    int r = uv_listen((uv_stream_t*) clientHandler->server, 128, &uv_onconnection);
    if (r) {
        fprintf(stderr, "Listen error %s\n", uv_strerror(r));
    }
}


int main(int argc, char* argv[]) {
  // Initialize V8.
  V8::InitializeICUDefaultLocation(argv[0]);
  V8::InitializeExternalStartupData(argv[0]);
  std::unique_ptr<Platform> platform = platform::NewDefaultPlatform();
  V8::InitializePlatform(platform.get());
  V8::Initialize();

  loop = uv_default_loop();
  // Create a new Isolate and make it the current one.
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = ArrayBuffer::Allocator::NewDefaultAllocator();
  Isolate* isolate = Isolate::New(create_params);
  {
    Isolate::Scope isolate_scope(isolate);
    // Create a stack-allocated handle scope.
    HandleScope handle_scope(isolate);
    Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
    // Bind the global 'print' function to the C++ Print callback.
    global->Set(isolate, "_log", FunctionTemplate::New(isolate, Print));
    global->Set(isolate, "_setTimeout", FunctionTemplate::New(isolate, RegisterTimeout));
    global->Set(isolate, "_setInterval", FunctionTemplate::New(isolate, RegisterInterval));
    global->Set(isolate, "_require", FunctionTemplate::New(isolate, Require));
    global->Set(isolate, "_existsSync", FunctionTemplate::New(isolate, ExistsSync));
    global->Set(isolate, "_listen", FunctionTemplate::New(isolate, Listen));
    global->Set(isolate, "module", ObjectTemplate::New(isolate));
    
    // Create a new context.
   Local<Context> context = Context::New(isolate, NULL, global);
    // Enter the context for compiling and running the hello world script.
   Context::Scope context_scope(context);
    // Create a string containing the JavaScript source code.
   Local<String> source = String::NewFromUtf8(isolate, loadScript("runtime/main.js"), NewStringType::kNormal).ToLocalChecked();
    // Compile the source code.
   Local<Script> script = Script::Compile(context, source).ToLocalChecked();
   Local<String> source2 = String::NewFromUtf8(isolate, loadScript("bootstrap.js"), NewStringType::kNormal).ToLocalChecked();
    // Compile the source code.
   Local<Script> script2 = Script::Compile(context, source2).ToLocalChecked();
    script2->Run(context).ToLocalChecked();

    // Run the script to get the result.
    Local<Value> result = script->Run(context).ToLocalChecked();
    // Convert the result to an UTF8 string and print it.
    String::Utf8Value utf8(isolate, result);
    //printf("%s\n", *utf8);
    cout << "Run event loop" << endl;

    uv_run(loop, UV_RUN_DEFAULT);
  }


  // Dispose the isolate and tear down V8.
  isolate->Dispose();
  V8::Dispose();
  V8::ShutdownPlatform();
  delete create_params.array_buffer_allocator;
  return 0;
}