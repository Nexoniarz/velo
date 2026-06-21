#ifndef _VELO_CONFIG_H
#define _VELO_CONFIG_H

/* ── Language identity ─────────────────────────────────────────────────── */
#define VELO_NAME             "Velo"
#define VELO_VERSION_MAJOR    1
#define VELO_VERSION_MINOR    0
#define VELO_VERSION_PATCH    0

/* ── Attribution ───────────────────────────────────────────────────────── */
#define VELO_AUTHOR           "Nexoniarz"
#define VELO_URL              "https://github.com/Nexoniarz/velo"
#define VELO_LICENSE          "Apache License 2.0"

/* ── File extensions ───────────────────────────────────────────────────── */
#define VELO_SOURCE_EXT       ".velo"
#define VELO_BYTECODE_EXT     ".veloc"

/* ── Derived strings (do not edit) ────────────────────────────────────── */
#define VELO_STR_(x)          #x
#define VELO_STR(x)           VELO_STR_(x)

#define VELO_VERSION_MM \
  VELO_STR(VELO_VERSION_MAJOR) "." VELO_STR(VELO_VERSION_MINOR)

#define VELO_VERSION \
  VELO_NAME " " VELO_VERSION_MM "." VELO_STR(VELO_VERSION_PATCH)

#define VELO_VERSION_NUM \
  (VELO_VERSION_MAJOR * 10000 + VELO_VERSION_MINOR * 100 + VELO_VERSION_PATCH)

#define VELO_COPYRIGHT \
  "Copyright (C) 2026 " VELO_AUTHOR \
  ". Based on LuaJIT \xA9 2005-2026 Mike Pall."

#endif /* _VELO_CONFIG_H */
