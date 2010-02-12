/**
 * Output the XML version of the data
 **/
function xmlsource()
{
    var xml = new XML(this.data);
    print(xml);
    return xml..a.@href;
}

/**
 * Output the data as-is
 **/
function source()
{
    print(this.data);
}

/**
 * Print out the meta-data of an HTML page
 **/
function meta()
{
    var xml = new XML(this.data);

    for each(var x in xml..meta) {
        print(x.@name+": ");
        println(x.@content);
    }

    return xml..a.@href;
}

/**
 * Print out the title of the HTML page
 **/
function title()
{
    var xml = new XML(this.data);
    println(xml..title);
    return xml..a.@href;
}
