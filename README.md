![logo](./Logo.png)

## Overview

Tapas is a programming language designed for mathematical modeling. 

[Documents](https://zhuanglinsheng.github.io/2024/08/28/Tapas.html)



## Features

- Header-Only
- Light
- Fast Speed



## Usage

Suppose we want to call the Tapas script in C++, then we can do it the following way: 

```c++
// file `test.cpp`
#include "Tapas/tapas.h"

int main(int argc, char ** args)
{
	tapas::tsession sess;  // create a Tapas session
	sess.compile_file("example.tap");  // compile Tapas scripy file
	sess.eval_bycodes("example.tap");  // evaluate Tapas source codes
	return 0;
}
```

where the Tapas script file `example.tap` looks like

```
// file `example.tap`
let abs = (x) {
	if (x >= 0) {
		return x
	}
	else {
		return -x
	}
}
abs(-2).std::print()
```

It should be easy to understand. Then, we just compile the C++ code by 

```sh
g++ test.cpp ./ -std=c++11 -I[path to the "include" folder] -o test
./test
```



