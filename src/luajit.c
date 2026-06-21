/*
** LuaJIT frontend. Runs commands, scripts, read-eval-print (REPL) etc.
** Copyright (C) 2005-2026 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define luajit_c

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "luajit.h"
#include "velo_config.h"

#include "lj_arch.h"

#if LJ_TARGET_POSIX
#include <unistd.h>
#define lua_stdin_is_tty()	isatty(0)
#elif LJ_TARGET_WINDOWS
#include <io.h>
#ifdef __BORLANDC__
#define lua_stdin_is_tty()	isatty(_fileno(stdin))
#else
#define lua_stdin_is_tty()	_isatty(_fileno(stdin))
#endif
#else
#define lua_stdin_is_tty()	1
#endif

#if !LJ_TARGET_CONSOLE
#include <signal.h>

#if LJ_TARGET_POSIX
/* Improve signal handling on POSIX. Try CTRL-C on: luajit -e 'io.read()' */
static void signal_set(int sig, void (*h)(int))
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = h;
  sigemptyset(&sa.sa_mask);
  sigaction(sig, &sa, NULL);
}
#else
#define signal_set	signal
#endif

#endif

static lua_State *globalL = NULL;
static const char *progname = LUA_PROGNAME;
static char *empty_argv[2] = { NULL, NULL };

#if !LJ_TARGET_CONSOLE
static void lstop(lua_State *L, lua_Debug *ar)
{
  (void)ar;  /* unused arg. */
  lua_sethook(L, NULL, 0, 0);
  /* Avoid luaL_error -- a C hook doesn't add an extra frame. */
  luaL_where(L, 0);
  lua_pushfstring(L, "%sinterrupted!", lua_tostring(L, -1));
  lua_error(L);
}

static void laction(int i)
{
  /* Terminate process if another SIGINT happens (double CTRL-C). */
  signal_set(i, SIG_DFL);
  lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}
#endif

static void print_usage(void)
{
  fputs("usage: ", stderr);
  fputs(progname, stderr);
  fputs(" [options]... [script [args]...].\n"
  "Available options are:\n"
  "  -e chunk         Execute string " LUA_QL("chunk") ".\n"
  "  -l name          Require library " LUA_QL("name") ".\n"
  "  -b ...           Save or list bytecode.\n"
  "  -j cmd           Perform JIT control command.\n"
  "  -O[opt]          Control JIT optimizations.\n"
  "  -i               Enter interactive mode after executing " LUA_QL("script") ".\n"
  "  -v               Show version information.\n"
  "  -E               Ignore environment variables.\n"
  "  --compile <src> [<out>]\n"
  "                   Compile script to bytecode (.veloc).\n"
  "  --lua            Run in interpreter mode (JIT disabled).\n"
  "  --               Stop handling options.\n"
  "  -                Execute stdin and stop handling options.\n", stderr);
  fflush(stderr);
}

static void l_message(const char *msg)
{
  if (progname) { fputs(progname, stderr); fputc(':', stderr); fputc(' ', stderr); }
  fputs(msg, stderr); fputc('\n', stderr);
  fflush(stderr);
}

static int report(lua_State *L, int status)
{
  if (status && !lua_isnil(L, -1)) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL) msg = "(error object is not a string)";
    l_message(msg);
    lua_pop(L, 1);
  }
  return status;
}

static int traceback(lua_State *L)
{
  if (!lua_isstring(L, 1)) { /* Non-string error object? Try metamethod. */
    if (lua_isnoneornil(L, 1) ||
	!luaL_callmeta(L, 1, "__tostring") ||
	!lua_isstring(L, -1))
      return 1;  /* Return non-string error object. */
    lua_remove(L, 1);  /* Replace object by result of __tostring metamethod. */
  }
  luaL_traceback(L, L, lua_tostring(L, 1), 1);
  return 1;
}

static int docall(lua_State *L, int narg, int clear)
{
  int status;
  int base = lua_gettop(L) - narg;  /* function index */
  lua_pushcfunction(L, traceback);  /* push traceback function */
  lua_insert(L, base);  /* put it under chunk and args */
#if !LJ_TARGET_CONSOLE
  signal_set(SIGINT, laction);
#endif
  status = lua_pcall(L, narg, (clear ? 0 : LUA_MULTRET), base);
#if !LJ_TARGET_CONSOLE
  signal_set(SIGINT, SIG_DFL);
#endif
  lua_remove(L, base);  /* remove traceback function */
  /* force a complete garbage collection in case of errors */
  if (status != LUA_OK) lua_gc(L, LUA_GCCOLLECT, 0);
  return status;
}

static void print_version(void)
{
  fputs("\n  " LUAJIT_VERSION "  |  " LUAJIT_LICENSE "\n"
	"  " LUAJIT_COPYRIGHT "\n"
	"  " LUAJIT_URL "\n\n", stdout);
}

static void print_jit_status(lua_State *L)
{
  int n;
  const char *s;
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, -1, "jit");  /* Get jit.* module table. */
  lua_remove(L, -2);
  lua_getfield(L, -1, "status");
  lua_remove(L, -2);
  n = lua_gettop(L);
  lua_call(L, 0, LUA_MULTRET);
  fputs(lua_toboolean(L, n) ? "JIT: ON" : "JIT: OFF", stdout);
  for (n++; (s = lua_tostring(L, n)); n++) {
    putc(' ', stdout);
    fputs(s, stdout);
  }
  putc('\n', stdout);
  lua_settop(L, 0);  /* clear stack */
}

static void createargtable(lua_State *L, char **argv, int argc, int argf)
{
  int i;
  lua_createtable(L, argc - argf, argf);
  for (i = 0; i < argc; i++) {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, -2, i - argf);
  }
  lua_setglobal(L, "arg");
}

static int dofile(lua_State *L, const char *name)
{
  int status = luaL_loadfile(L, name) || docall(L, 0, 1);
  return report(L, status);
}

static int dostring(lua_State *L, const char *s, const char *name)
{
  int status = luaL_loadbuffer(L, s, strlen(s), name) || docall(L, 0, 1);
  return report(L, status);
}

static int dolibrary(lua_State *L, const char *name)
{
  lua_getglobal(L, "require");
  lua_pushstring(L, name);
  return report(L, docall(L, 1, 1));
}

static void write_prompt(lua_State *L, int firstline)
{
  const char *p;
  lua_getfield(L, LUA_GLOBALSINDEX, firstline ? "_PROMPT" : "_PROMPT2");
  p = lua_tostring(L, -1);
  if (p == NULL) p = firstline ? LUA_PROMPT : LUA_PROMPT2;
  fputs(p, stdout);
  fflush(stdout);
  lua_pop(L, 1);  /* remove global */
}

static int incomplete(lua_State *L, int status)
{
  if (status == LUA_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = lua_tolstring(L, -1, &lmsg);
    const char *tp = msg + lmsg - (sizeof(LUA_QL("<eof>")) - 1);
    if (strstr(msg, LUA_QL("<eof>")) == tp) {
      lua_pop(L, 1);
      return 1;
    }
  }
  return 0;  /* else... */
}

static int pushline(lua_State *L, int firstline)
{
  char buf[LUA_MAXINPUT];
  write_prompt(L, firstline);
  if (fgets(buf, LUA_MAXINPUT, stdin)) {
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n')
      buf[len-1] = '\0';
    if (firstline && buf[0] == '=')
      lua_pushfstring(L, "return %s", buf+1);
    else
      lua_pushstring(L, buf);
    return 1;
  }
  return 0;
}

static int loadline(lua_State *L)
{
  int status;
  lua_settop(L, 0);
  if (!pushline(L, 1))
    return -1;  /* no input */
  for (;;) {  /* repeat until gets a complete line */
    status = luaL_loadbuffer(L, lua_tostring(L, 1), lua_strlen(L, 1), "=stdin");
    if (!incomplete(L, status)) break;  /* cannot try to add lines? */
    if (!pushline(L, 0))  /* no more input? */
      return -1;
    lua_pushliteral(L, "\n");  /* add a new line... */
    lua_insert(L, -2);  /* ...between the two lines */
    lua_concat(L, 3);  /* join them */
  }
  lua_remove(L, 1);  /* remove line */
  return status;
}

static void dotty(lua_State *L)
{
  int status;
  const char *oldprogname = progname;
  progname = NULL;
  while ((status = loadline(L)) != -1) {
    if (status == LUA_OK) status = docall(L, 0, 0);
    report(L, status);
    if (status == LUA_OK && lua_gettop(L) > 0) {  /* any result to print? */
      lua_getglobal(L, "print");
      lua_insert(L, 1);
      if (lua_pcall(L, lua_gettop(L)-1, 0, 0) != 0)
	l_message(lua_pushfstring(L, "error calling " LUA_QL("print") " (%s)",
				  lua_tostring(L, -1)));
    }
  }
  lua_settop(L, 0);  /* clear stack */
  fputs("\n", stdout);
  fflush(stdout);
  progname = oldprogname;
}

static int handle_script(lua_State *L, char **argx)
{
  int status;
  const char *fname = argx[0];
  if (strcmp(fname, "-") == 0 && strcmp(argx[-1], "--") != 0)
    fname = NULL;  /* stdin */
  status = luaL_loadfile(L, fname);
  if (status == LUA_OK) {
    /* Fetch args from arg table. LUA_INIT or -e might have changed them. */
    int narg = 0;
    lua_getglobal(L, "arg");
    if (lua_istable(L, -1)) {
      do {
	narg++;
	lua_rawgeti(L, -narg, narg);
      } while (!lua_isnil(L, -1));
      lua_pop(L, 1);
      lua_remove(L, -narg);
      narg--;
    } else {
      lua_pop(L, 1);
    }
    status = docall(L, narg, 0);
  }
  return report(L, status);
}

/* Load add-on module. */
static int loadjitmodule(lua_State *L)
{
  lua_getglobal(L, "require");
  lua_pushliteral(L, "jit.");
  lua_pushvalue(L, -3);
  lua_concat(L, 2);
  if (lua_pcall(L, 1, 1, 0)) {
    const char *msg = lua_tostring(L, -1);
    if (msg && !strncmp(msg, "module ", 7))
      goto nomodule;
    return report(L, 1);
  }
  lua_getfield(L, -1, "start");
  if (lua_isnil(L, -1)) {
  nomodule:
    l_message("unknown Velo/JIT command or jit.* modules not installed");
    return 1;
  }
  lua_remove(L, -2);  /* Drop module table. */
  return 0;
}

/* Run command with options. */
static int runcmdopt(lua_State *L, const char *opt)
{
  int narg = 0;
  if (opt && *opt) {
    for (;;) {  /* Split arguments. */
      const char *p = strchr(opt, ',');
      narg++;
      if (!p) break;
      if (p == opt)
	lua_pushnil(L);
      else
	lua_pushlstring(L, opt, (size_t)(p - opt));
      opt = p + 1;
    }
    if (*opt)
      lua_pushstring(L, opt);
    else
      lua_pushnil(L);
  }
  return report(L, lua_pcall(L, narg, 0, 0));
}

/* JIT engine control command: try jit library first or load add-on module. */
static int dojitcmd(lua_State *L, const char *cmd)
{
  const char *opt = strchr(cmd, '=');
  lua_pushlstring(L, cmd, opt ? (size_t)(opt - cmd) : strlen(cmd));
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, -1, "jit");  /* Get jit.* module table. */
  lua_remove(L, -2);
  lua_pushvalue(L, -2);
  lua_gettable(L, -2);  /* Lookup library function. */
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);  /* Drop non-function and jit.* table, keep module name. */
    if (loadjitmodule(L))
      return 1;
  } else {
    lua_remove(L, -2);  /* Drop jit.* table. */
  }
  lua_remove(L, -2);  /* Drop module name. */
  return runcmdopt(L, opt ? opt+1 : opt);
}

/* Optimization flags. */
static int dojitopt(lua_State *L, const char *opt)
{
  lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
  lua_getfield(L, -1, "jit.opt");  /* Get jit.opt.* module table. */
  lua_remove(L, -2);
  lua_getfield(L, -1, "start");
  lua_remove(L, -2);
  return runcmdopt(L, opt);
}

/* Save or list bytecode. */
static int dobytecode(lua_State *L, char **argv)
{
  int narg = 0;
  lua_pushliteral(L, "bcsave");
  if (loadjitmodule(L))
    return 1;
  if (argv[0][2]) {
    narg++;
    argv[0][1] = '-';
    lua_pushstring(L, argv[0]+1);
  }
  for (argv++; *argv != NULL; narg++, argv++)
    lua_pushstring(L, *argv);
  report(L, lua_pcall(L, narg, 0, 0));
  return -1;
}

/* check that argument has no extra characters at the end */
#define notail(x)	{if ((x)[2] != '\0') return -1;}

#define FLAGS_INTERACTIVE	1
#define FLAGS_VERSION		2
#define FLAGS_EXEC		4
#define FLAGS_OPTION		8
#define FLAGS_NOENV		16
#define FLAGS_COMPILE		32
#define FLAGS_LUA		64
#define FLAGS_EXEC_NATIVE	128

/* Memory buffer for lua_dump when building a native executable. */
typedef struct { char *buf; size_t size; size_t cap; } ByteBuf;

static int bytebuf_writer(lua_State *L, const void *p, size_t sz, void *ud)
{
  ByteBuf *b = (ByteBuf *)ud;
  (void)L;
  if (b->size + sz > b->cap) {
    size_t newcap = (b->cap ? b->cap * 2 : 65536);
    while (newcap < b->size + sz) newcap *= 2;
    char *newbuf = (char *)realloc(b->buf, newcap);
    if (!newbuf) return 1;
    b->buf = newbuf;
    b->cap = newcap;
  }
  memcpy(b->buf + b->size, p, sz);
  b->size += sz;
  return 0;
}

/* Writer callback for lua_dump when writing directly to a file. */
static int compile_writer(lua_State *L, const void *p, size_t sz, void *ud)
{
  (void)L;
  return fwrite(p, sz, 1, (FILE *)ud) != 1;
}

/* Derive the output name from the input: strip extension, add suffix. */
static int derive_outname(const char *fname, const char *suffix,
			   char *buf, size_t bufsz)
{
  const char *dot = strrchr(fname, '.');
  const char *slash = strrchr(fname, '/');
  size_t base_len;
  size_t slen = strlen(suffix);
  if (dot && (!slash || dot > slash))
    base_len = (size_t)(dot - fname);
  else
    base_len = strlen(fname);
  if (base_len + slen + 1 > bufsz) return 0;
  memcpy(buf, fname, base_len);
  memcpy(buf + base_len, suffix, slen + 1);
  return 1;
}

/* Compile source to a native standalone executable via a C stub. */
static int docompile_exec(lua_State *L, const char *fname, const char *out)
{
  ByteBuf bb = { NULL, 0, 0 };
  FILE *cfp;
  char tmp_c[512], tmp_main[512];
  char *cmd = NULL;
  size_t i;
  int ret;

  /* Load the Velo source into bytecode in memory. */
  if (luaL_loadfile(L, fname) != LUA_OK) {
    int r = report(L, 1); return r;
  }
  if (lua_dump(L, bytebuf_writer, &bb) != 0 || !bb.buf) {
    lua_pop(L, 1);
    l_message("--compile --exec: bytecode dump failed");
    return 1;
  }
  lua_pop(L, 1);

  /* Write bytecode as a C byte array. */
  snprintf(tmp_c, sizeof(tmp_c), "/tmp/velo_bc_%d.c", (int)getpid());
  cfp = fopen(tmp_c, "w");
  if (!cfp) { free(bb.buf); l_message("--compile --exec: cannot create temp file"); return 1; }
  fputs("const char velo_bytecode[] = {\n  ", cfp);
  for (i = 0; i < bb.size; i++) {
    fprintf(cfp, "0x%02x", (unsigned char)bb.buf[i]);
    if (i + 1 < bb.size) {
      fputs(",", cfp);
      if ((i + 1) % 16 == 0) fputs("\n  ", cfp); else fputs(" ", cfp);
    }
  }
  fputs("\n};\n", cfp);
  fprintf(cfp, "const unsigned int velo_bytecode_len = %u;\n", (unsigned)bb.size);
  fclose(cfp);
  free(bb.buf);

  /* Write the stub main.c. */
  snprintf(tmp_main, sizeof(tmp_main), "/tmp/velo_main_%d.c", (int)getpid());
  cfp = fopen(tmp_main, "w");
  if (!cfp) { remove(tmp_c); l_message("--compile --exec: cannot create stub file"); return 1; }
  fputs(
    "#include <stdio.h>\n"
    "#include <stdlib.h>\n"
    "#include \"lua.h\"\n"
    "#include \"lauxlib.h\"\n"
    "#include \"lualib.h\"\n"
    "extern const char velo_bytecode[];\n"
    "extern const unsigned int velo_bytecode_len;\n"
    "int main(int argc, char **argv) {\n"
    "  int i; lua_State *L = luaL_newstate();\n"
    "  if (!L) { fputs(\"velo: out of memory\\n\", stderr); return 1; }\n"
    "  luaL_openlibs(L);\n"
    "  lua_createtable(L, argc, 0);\n"
    "  for (i = 0; i < argc; i++) {\n"
    "    lua_pushstring(L, argv[i]); lua_rawseti(L, -2, i); }\n"
    "  lua_setglobal(L, \"arg\");\n"
    "  if (luaL_loadbuffer(L, velo_bytecode, velo_bytecode_len, \"main\") != 0 ||\n"
    "      lua_pcall(L, 0, 0, 0) != 0) {\n"
    "    fprintf(stderr, \"%s\\n\", lua_tostring(L, -1));\n"
    "    lua_close(L); return 1;\n"
    "  }\n"
    "  lua_close(L); return 0;\n"
    "}\n", cfp);
  fclose(cfp);

  /* Locate the directory of this binary via /proc/self/exe. */
  char bindir[4096];
  {
    ssize_t n = readlink("/proc/self/exe", bindir, sizeof(bindir)-1);
    if (n > 0) {
      bindir[n] = '\0';
      char *sl = strrchr(bindir, '/');
      if (sl) *sl = '\0'; else bindir[0] = '\0';
    } else {
      bindir[0] = '\0';
    }
  }
  /* Build compile command: pass libluajit.a by path to avoid picking up the
   * system shared library. --whole-archive pulls in lib_velo.o (Velo runtime). */
  if (asprintf(&cmd,
        "cc -O2 -o '%s' '%s' '%s'"
        " -I'%s' -Wl,--whole-archive '%s/libluajit.a' -Wl,--no-whole-archive"
        " -ldl -lm",
        out, tmp_main, tmp_c, bindir, bindir) < 0 || !cmd) {
    remove(tmp_c);
    remove(tmp_main);
    l_message("--compile --exec: out of memory building compile command");
    return 1;
  }
  ret = system(cmd);
  free(cmd);
  remove(tmp_c);
  remove(tmp_main);
  if (ret != 0) {
    l_message("--compile --exec: linking failed (is cc in PATH and libluajit available?)");
    return 1;
  }
  fprintf(stdout, "Compiled: %s -> %s (native executable)\n", fname, out);
  return -1;
}

/* Compile a Velo source file to bytecode (.veloc) or native executable. */
static int docompile(lua_State *L, char **argv, int i, int make_exec)
{
  /* Skip sub-flags like --exec that may appear right after --compile. */
  int j = i + 1;
  while (argv[j] && argv[j][0] == '-' && argv[j][1] == '-') {
    if (strcmp(argv[j], "--exec") == 0) { make_exec = 1; j++; }
    else break;
  }
  const char *fname = argv[j];
  char outbuf[4096];
  const char *out;
  FILE *fp;
  int status;

  if (!fname) {
    l_message("--compile: missing input file");
    return 1;
  }

  if (argv[j+1] && argv[j+1][0] != '-') {
    out = argv[j+1];
  } else {
    const char *suffix = make_exec ? "" : VELO_BYTECODE_EXT;
    if (!derive_outname(fname, suffix, outbuf, sizeof(outbuf))) {
      l_message("--compile: filename too long");
      return 1;
    }
    out = outbuf;
  }

  if (make_exec)
    return docompile_exec(L, fname, out);

  status = luaL_loadfile(L, fname);
  if (status != LUA_OK) return report(L, status);

  fp = fopen(out, "wb");
  if (!fp) {
    l_message("--compile: cannot open output file");
    lua_pop(L, 1);
    return 1;
  }
  if (lua_dump(L, compile_writer, fp) != 0) {
    fclose(fp);
    l_message("--compile: failed to write bytecode");
    lua_pop(L, 1);
    return 1;
  }
  fclose(fp);
  lua_pop(L, 1);

  fprintf(stdout, "Compiled: %s -> %s\n", fname, out);
  return -1;
}

static int collectargs(char **argv, int *flags)
{
  int i;
  for (i = 1; argv[i] != NULL; i++) {
    if (argv[i][0] != '-')  /* Not an option? */
      return i;
    switch (argv[i][1]) {  /* Check option. */
    case '-':
      if (strcmp(argv[i], "--compile") == 0) {
	if (*flags & ~(FLAGS_EXEC_NATIVE)) return -1;
	*flags |= FLAGS_COMPILE | FLAGS_EXEC;
	return i+1;
      }
      if (strcmp(argv[i], "--exec") == 0) {
	*flags |= FLAGS_EXEC_NATIVE;
	break;
      }
      if (strcmp(argv[i], "--lua") == 0) {
	*flags |= FLAGS_LUA;
	break;
      }
      notail(argv[i]);
      return i+1;
    case '\0':
      return i;
    case 'i':
      notail(argv[i]);
      *flags |= FLAGS_INTERACTIVE;
      /* fallthrough */
    case 'v':
      notail(argv[i]);
      *flags |= FLAGS_VERSION;
      break;
    case 'e':
      *flags |= FLAGS_EXEC;
      /* fallthrough */
    case 'j':  /* LuaJIT extension */
    case 'l':
      *flags |= FLAGS_OPTION;
      if (argv[i][2] == '\0') {
	i++;
	if (argv[i] == NULL) return -1;
      }
      break;
    case 'O': break;  /* LuaJIT extension */
    case 'b':  /* LuaJIT extension */
      if (*flags) return -1;
      *flags |= FLAGS_EXEC;
      return i+1;
    case 'E':
      *flags |= FLAGS_NOENV;
      break;
    default: return -1;  /* invalid option */
    }
  }
  return i;
}

static int runargs(lua_State *L, char **argv, int argn, int flags)
{
  int i;
  for (i = 1; i < argn; i++) {
    if (argv[i] == NULL) continue;
    lua_assert(argv[i][0] == '-');
    switch (argv[i][1]) {
    case 'e': {
      const char *chunk = argv[i] + 2;
      if (*chunk == '\0') chunk = argv[++i];
      lua_assert(chunk != NULL);
      if (dostring(L, chunk, "=(command line)") != 0)
	return 1;
      break;
      }
    case 'l': {
      const char *filename = argv[i] + 2;
      if (*filename == '\0') filename = argv[++i];
      lua_assert(filename != NULL);
      if (dolibrary(L, filename))
	return 1;
      break;
      }
    case 'j': {  /* LuaJIT extension. */
      const char *cmd = argv[i] + 2;
      if (*cmd == '\0') cmd = argv[++i];
      lua_assert(cmd != NULL);
      if (dojitcmd(L, cmd))
	return 1;
      break;
      }
    case 'O':  /* LuaJIT extension. */
      if (dojitopt(L, argv[i] + 2))
	return 1;
      break;
    case 'b':  /* LuaJIT extension. */
      return dobytecode(L, argv+i);
    case '-':  /* Long options handled in pmain or here. */
      if (strcmp(argv[i], "--compile") == 0)
	return docompile(L, argv, i, flags & FLAGS_EXEC_NATIVE);
      break;  /* --exec, --lua, and bare -- are handled elsewhere */
    default: break;
    }
  }
  return LUA_OK;
}

static int handle_luainit(lua_State *L)
{
#if LJ_TARGET_CONSOLE
  const char *init = NULL;
#else
  const char *init = getenv(LUA_INIT);
#endif
  if (init == NULL)
    return LUA_OK;
  else if (init[0] == '@')
    return dofile(L, init+1);
  else
    return dostring(L, init, "=" LUA_INIT);
}

static struct Smain {
  char **argv;
  int argc;
  int status;
} smain;

static int pmain(lua_State *L)
{
  struct Smain *s = &smain;
  char **argv = s->argv;
  int argn;
  int flags = 0;
  globalL = L;
  LUAJIT_VERSION_SYM();  /* Linker-enforced version check. */

  argn = collectargs(argv, &flags);
  if (argn < 0) {  /* Invalid args? */
    print_usage();
    s->status = 1;
    return 0;
  }

  if ((flags & FLAGS_NOENV)) {
    lua_pushboolean(L, 1);
    lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
  }

  /* Stop collector during library initialization. */
  lua_gc(L, LUA_GCSTOP, 0);
  luaL_openlibs(L);
  lua_gc(L, LUA_GCRESTART, -1);

  if (flags & FLAGS_LUA) {
    /* --lua: disable JIT tracing, run in pure interpreter mode. */
    luaJIT_setmode(L, 0, LUAJIT_MODE_ENGINE | LUAJIT_MODE_OFF);
  }

  createargtable(L, argv, s->argc, argn);

  if (!(flags & FLAGS_NOENV)) {
    s->status = handle_luainit(L);
    if (s->status != LUA_OK) return 0;
  }

  if ((flags & FLAGS_VERSION)) print_version();

  s->status = runargs(L, argv, argn, flags);
  if (s->status != LUA_OK) return 0;

  if (s->argc > argn) {
    s->status = handle_script(L, argv + argn);
    if (s->status != LUA_OK) return 0;
  }

  if ((flags & FLAGS_INTERACTIVE)) {
    print_jit_status(L);
    dotty(L);
  } else if (s->argc == argn && !(flags & (FLAGS_EXEC|FLAGS_VERSION))) {
    if (lua_stdin_is_tty()) {
      print_version();
      print_jit_status(L);
      dotty(L);
    } else {
      dofile(L, NULL);  /* Executes stdin as a file. */
    }
  }
  return 0;
}

int main(int argc, char **argv)
{
  int status;
  lua_State *L;
  if (!argv[0]) argv = empty_argv; else if (argv[0][0]) progname = argv[0];
  L = lua_open();
  if (L == NULL) {
    l_message("cannot create state: not enough memory");
    return EXIT_FAILURE;
  }
  smain.argc = argc;
  smain.argv = argv;
  status = lua_cpcall(L, pmain, NULL);
  report(L, status);
  lua_close(L);
  return (status || smain.status > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

