
function wikipedia_search_init(q)
{
    var ret = Array();

    for (var x=0; x<q.length; x++)
        ret.push("http://en.wikipedia.org/w/index.php?search="+q[x]+"&fulltext=Advanced+search");

    return ret;
}

function parser()
{
    var xml = new XML(this.data);
    var uls = xml..ul;

    for each(var ul in uls) {
        if (ul.@class == 'mw-search-results') {
            for each(var li in ul.li) {
                println(li.a.@title+': http://en.wikipedia.org'+li.a.@href);
            }
        }
    }
    return null;
}

