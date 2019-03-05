Math.random = function(){
	return 0;
};

Date.now = function() {
	return 0;
};

var bind = Function.bind;
var unbind = bind.bind(bind);

function instantiate(constructor, args) {
    return new (unbind(constructor, null).apply(null, args));
}

Date = function (Date) {
    var names = Object.getOwnPropertyNames(Date);
    for (var i = 0; i < names.length; i++) {
        // Skip props already in the MyDate object
        if (names[i] in MyDate) continue;
        var desc = Object.getOwnPropertyDescriptor(Date, names[i]);
        Object.defineProperty(MyDate, names[i], desc);
    }

    return MyDate;

    function MyDate() {
    	if (arguments.length == 0) {
    		arguments = [0];
    	}
    	return instantiate(Date, arguments);
    }
}(Date);
