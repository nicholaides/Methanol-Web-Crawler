function init(args)
{
    var ret = Array();

    for (var x=0; x<args.length; x++) {
        ret.push("http://www.youtube.com/results?search_query="+args[x]+"&search_type=&aq=f");
    }

    return ret;
}

function youtube()
{
    var xml = new XML(this.data);

    for each(var x in xml..div) {
        if (x.@class == "video-long-title") {
            println(x.a[0].@title);
            println("http://www.youtube.com"+x.a[0].@href);
        }
    }
}
