// scoot: this should work fine on linux, though it displeases me
extern "C" {
#ifdef _MSC_VER

#ifdef BOILER_EXPORT
#define BOILER_API __declspec(dllexport)
#else
#define BOILER_API __declspec(dllimport)
#endif

#else
#define BOILER_API __attribute__((visibility("default")))
#endif
BOILER_API int boiler_init();

BOILER_API const char * boiler_get_username();

}