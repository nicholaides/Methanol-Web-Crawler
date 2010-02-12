function validator()
{
    var xml = get("http://validator.w3.org/check?output=soap12&uri="+this.url);

    xml = xml.substring(xml.indexOf("<env:"));
    xml = new XML(xml);
    var env = new Namespace("http://www.w3.org/2003/05/soap-envelope");
    var m   = new Namespace("http://www.w3.org/2005/10/markup-validator");

    var num_errors   = parseInt(xml.env::Body..m::errorcount);
    var num_warnings = parseInt(xml.env::Body..m::warningcount);

    println("Results for URL: "+this.url);
    println("Errors: "+num_errors);
    println("Warnings: "+num_warnings);
    if (num_errors || num_warnings)
        println("Further info: http://validator.w3.org/check?uri="+this.url);

    xml = new XML(this.data);
    return xml..a.@href;
}
