// file `test.cpp`
#include "Tapas/tapas.h"

int main(int argc, char ** args)
{
	tapas::tsession sess;  // create a Tapas session
	sess.compile_file("example.tap");  // compile Tapas scripy file
	sess.eval_bycodes("example.tap");  // evaluate Tapas source codes
	return 0;
}
