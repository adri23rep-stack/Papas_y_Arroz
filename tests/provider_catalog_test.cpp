#include "provider_catalog.hpp"
#include <cassert>
#include <string>
int main(){
    using xs::provider_for_catalog_label;
    assert(provider_for_catalog_label("Netflix Películas")=="netflix");
    assert(provider_for_catalog_label("HBO MAX / Series")=="hbo_max");
    assert(provider_for_catalog_label("Apple TV+ Originals")=="apple_tv");
    assert(provider_for_catalog_label("YouTube")=="youtube");
    assert(provider_for_catalog_label("Disney Plus")=="disney");
    assert(provider_for_catalog_label("Prime Video / Peliculas")=="prime_video");
    assert(provider_for_catalog_label("Paramount+")== "paramount");
    assert(provider_for_catalog_label("ViX Premium")== "vix");
    assert(provider_for_catalog_label("Pluto TV Canales")== "pluto_tv");
    assert(provider_for_catalog_label("HULU series")== "hulu");
    assert(provider_for_catalog_label("Maximo rendimiento").empty());
    assert(provider_for_catalog_label("Apple película").empty());
    assert(provider_for_catalog_label("Categoría acción").empty());
    assert(xs::provider_title("netflix")!=nullptr);
    assert(xs::provider_title("unknown")==nullptr);
    return 0;
}
