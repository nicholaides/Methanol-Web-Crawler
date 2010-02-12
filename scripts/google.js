/**
 * This function will be called before any crawling is done, 
 * if you run methabot from command line this function will 
 * receive the extra arguments you give methabot and compose
 * google search strings from them.
 **/
function google_search_init(q)
{
    var ret = Array();

    for (var x=0; x<q.length; x++)
        ret.push("http://www.google.com/search?q="+q[x]+"&start=0");

    return ret;
}

function parser()
{
    var xml = new XML(this.data);
    var urls = xml..a;
    var ret = Array();

    for each(var url in urls) {
        if (url.@class == "l")
            ret.push(url.@href);
    }

    return ret;
}

