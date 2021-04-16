// scoot: this should work fine on linux, though it displeases me
extern "C" {

#ifdef BOILER_EXPORT
#define BOILER_API __declspec(dllexport)
#else
#define BOILER_API __declspec(dllimport)
#endif

BOILER_API int boiler_init();

BOILER_API const char * boiler_get_username();

}