#include <qwaq/globals.rh>
#include <qwaq/fields.rh>
#include <qwaq/builtins.rh>

#define PI 3.14159265358973

/* Print a silly conversion table between degrees and radians */
void () main =
{
    float   degrees, radians;
    integer lower, upper, step;

    lower = 0;               // lower limit
    upper = 360;             // upper limit
    step = 45;               // step size
 
    degrees = lower;
    while (degrees <= upper) {
        radians = degrees * (PI / 180);
        printf ("%f\t%f\n", degrees, radians);
        degrees = degrees + step;
    }
};