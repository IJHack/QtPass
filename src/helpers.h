#ifndef HELPERS_H
#define HELPERS_H

#if __cplusplus >= 201703L
#define AS_CONST(x) std::as_const(x)
#else
#define AS_CONST(x) qAsConst(x)
#endif

#endif // HELPERS_H
