// scoot: this should work fine on linux, though it displeases me
#ifdef BOILER_EXPORT
#define BOILER_API __declspec(dllexport)
#else
#define BOILER_API __declspec(dllimport)
#endif
extern "C" {

void BOILER_API boiler_init();

}