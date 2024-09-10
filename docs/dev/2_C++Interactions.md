---
layout: post
title: "Tapas Programming Language"
use_math: false
---



# 2.2. C++ Interaction

## 2.2.1. Calling Tapas in C++

The APIs of calling Tapas in C++ are defined in the class ``tapas::tsession`` in ``tap.h`` file, which provides a set of methods. The most important two are:

- ``tsession::compile_file(const std::string & file_path)``
- ``tsession::eval_bycodes(const std::string & file_path)``

Tapas source code file should be stored in a text file with suffix ``.tap``. By providing the location of Tapas source code file, we just

- Import the header ``tap.h`` from the ``src`` folder, and
- Call in C++ the above two method two execute it.

For example, suppose you have a Tapas script ``test_calling.tap`` inside which looks like

```tapas
var abs = (x) {
	if (x >= 0) {
		return x
	}
	else {
		return -x
	}
}
abs(-2).print()
```

Then we call this ``test_calling.tap`` in C++ by

```c++
#include "tap.h"

int main(int argc, char ** args)
{
	// create a Tapas session
	tapas::tsession sess;

	// compile Tapas source codes
	sess.compile_file("test_calling.tap");

	// evaluate Tapas source codes
	sess.eval_bycodes("test_calling.tap");
	return 0;
}
```

<br><br>

## 2.2.2. Extend Tapas with C++ (1)

You can expand the default functions of Tapas script by adding C++ functions into it. In order to do this, you need to

- Create your C++ function of ``tapas::cppf`` type, that is a function type defined as

```c++
typedef void (*cppf)(Tap::Tapv *const, unsigned char, Tap::Tapv&);
```

- Register your C++ function to your Tapas session by calling the method

```c++
  tsession::add_cppf(const std::string & fname, const cppf & f)
```

As an example, we define a C++ function in ``test_Tap_extension.h``, which looks like

```c++
#include "./tap-script/src/tap.h"
using namespace tap;

void cpp_int_sum(tapv * const params, unsigned char len, tapv & vre)
{
	long s = 0;
	tapas::tapv * param_i = params;

	for (unsigned char i = 0; i < len; i++) {
		if (params->get_type() != tapas::tint) {
			vre.set_type(tapas::tnil);
			return;
		} else 
			s += param_i->get_v_tint();
		param_i++;
	}
	vre.set_type(tapas::tint);
	vre.set_v_tint(s);
}
```

Clearly, the function ``cpp_int_sum`` defined above follows the signature of functions of ``cppf`` type. We want to use this C++ function in our Tapas file ``test_Tap_extension.tap``, which is given below

```tapas
// calling int_sum linking to 'cpp_int_sum'
var s = int_sum(1,2,3,4,5)

// print out thr result
print(s)
```

All we need to do is to register this C++ function in the current Tapas session. So, let's do it. Let's execute ``test_Tap_extension.tap`` which calls ``int_sum``,

```c++
#include <string>
#include "tap.h"
#include "test_Tap_extension.h"

int main()
{
	// create a Tapas session
	tapas::tsession sess;

	// register this C++ function
	sess.add_cppf("int_sum", cpp_int_sum);
	std::string src_codes = "test_Tap_extension.tap";

	// compile the Tapas source code
	sess.compile_file(src_codes);

	// execute the Tapas source code
	sess.eval_bycodes(src_codes);
	return 0;
}
```

And we should get the return ``15``.

<br><br>


## 2.2.3. Extend Tapas with C++ (2)

In order to extend the data structures in Tapas script, we need to make C++ class to inherit ``tapas::tcompo_v``, which is a virtual class asking for the implementations of methods ``copy`` and ``print``. If further attributes are needed (callable, indexable, iterable, etc.), then we can inherit ``tcompo_eval``, ``tcompo_idx`` and ``tcompo_iter``, and implement their methods. After creating the class that we need, we also need to create a ``cppfunc`` so that we can use this data structure in Tapas script (see above).
