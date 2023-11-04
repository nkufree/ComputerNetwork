#include "defs.h"
using namespace std;
char stateName[][15] = {
    "CLOSED", "LISTEN", "SYN_SENT", "SYN_RCVD", "ESTABLISHED", "CLOSE_WAIT", "FIN_WAIT_1", "LAST_ACK", "FIN_WAIT_2"
};

void handleError(RC rc)
{
    switch(rc)
    {
        case RC::CHECK_ERROR:
            cout << "[ check error ] ";
            break;
        case RC::INTERNAL:
            cout << "[ falied ] ";
            break;
        case RC::SEQ_ERROR:
            cout << "[ seq error ] ";
            break;
        case RC::SOCK_ERROR:
            cout << "[ socket error ] ";
            break;
        case RC::STATE_ERROR:
            cout << "[ state error ] ";
            break;
        case RC::WAIT_TIME_ERROR:
            cout << "[ wait time error ] ";
            break;
        case RC::UNIMPLENTED:
            cout << "[ unimplented ]";
            break;
        default:
            break;
    }
}