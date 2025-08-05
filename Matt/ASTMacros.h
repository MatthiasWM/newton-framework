
/*
 File:    Matt/ASTMacros.h

 Matt's decompiler Abstract Syntax Tree Macros

 Written by:  Matt, 2025.
 */

#if !defined(__MATT_AST_MACROS_H)
#define __MATT_AST_MACROS_H 1

#define REQUIRED_COND(type, name, cond, iter, resolved) \
  type *name=nullptr; \
  name = dynamic_cast<type*>(iter); \
if (!name || (resolved && !name->Resolved()) || !(cond)) { name=nullptr; break; }

#define REQUIRED_NODE(type, name, iter, resolved) \
  type *name=nullptr; \
  name = dynamic_cast<type*>(iter); \
  if (!name || (resolved && !name->Resolved())) { name=nullptr; break; }

#define OPTIONAL_COND(type, name, cond, iter, resolved) \
  type *name=nullptr; \
  name = dynamic_cast<type*>(iter); \
  if (!name || (resolved && !name->Resolved()) || !(cond)) { name=nullptr; } else

#define OPTIONAL_NODE(type, name, iter, resolved) \
  type *name=nullptr; \
  name = dynamic_cast<type*>(iter); \
  if (!name || (resolved && !name->Resolved())) { name=nullptr; } else

//#define REQUIRED_NODE_IF(cond, type, name, iter) \
//type *name=nullptr; if (cond) name = dynamic_cast<type*>(iter); if ((cond) && !name) break;
//
//#define OPRTIONAL_NODE_IF(cond, type, name, iter) \
//type *name=nullptr; if (cond) name = dynamic_cast<type*>(iter); if ((cond) && name)

#endif  /* __MATT_AST_MACROS_H */
