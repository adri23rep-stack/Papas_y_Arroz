#include "nova_catalog_cache.hpp"
#include <QTemporaryDir>
int main(){
    QTemporaryDir temp;
    if(!temp.isValid())return 1;
    xs::Catalog page;
    xs::Entry e;e.kind=xs::EntryKind::Episode;e.title="Prueba";e.content_id="episode";
    e.source="https://private.invalid/token";e.artwork=e.source;
    page.entries.push_back(e);e.kind=xs::EntryKind::Media;page.entries.push_back(e);
    nova_cache::save(temp.path(),"section",page);
    QFile bytes(temp.filePath("section.json"));
    if(!bytes.open(QIODevice::ReadOnly)||bytes.readAll().contains("private.invalid"))return 2;
    bytes.close();
    auto restored=nova_cache::read(temp.path(),"section");
    if(!restored||restored->entries.size()!=1||restored->entries.front().content_id!="episode")return 3;
    nova_cache::save(temp.path(),"section",{});
    restored=nova_cache::read(temp.path(),"section");
    if(!restored||!restored->entries.empty())return 4;
    if(!bytes.open(QIODevice::WriteOnly))return 5;
    bytes.write("{");bytes.close();
    return nova_cache::read(temp.path(),"section")?6:0;
}
