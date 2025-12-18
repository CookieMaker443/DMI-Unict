#include <iostream>
#include <time.h>
#include <cstdlib>

using namespace std;

struct t_nodo
{
  int info;
  t_nodo *sx;
  double costo_sx;

  t_nodo *dx;
  double costo_dx;

};

t_nodo *crea_albero_bilanciato(int quanti_nodi)
{
  if(quanti_nodi==0)  //base della ricorsione ...
    return nullptr;
  else
  {

    //crea il nodo a questo livello
    t_nodo *nuovo = new t_nodo;
    nuovo->info = rand()%100;

    int quanti_a_sx = quanti_nodi/2;
    int quanti_a_dx = quanti_nodi - quanti_a_sx - 1;

    nuovo->sx = crea_albero_bilanciato(quanti_a_sx);  //passo ricorsivo
    nuovo->dx = crea_albero_bilanciato(quanti_a_dx);  //passo ricorsivo

    return nuovo;
  }
}

void stampa_albero_binario_pre_order(t_nodo *p)
{
  if (p!=nullptr)
  {
     cout << p->info << endl;

     stampa_albero_binario_pre_order(p->sx);
     stampa_albero_binario_pre_order(p->dx);
  }
}

void stampa_albero_binario_in_order(t_nodo *p)
{
  if (p!=nullptr)
  {
     stampa_albero_binario_in_order(p->sx);

     cout << p->info << endl;

     stampa_albero_binario_in_order(p->dx);
  }
}


void stampa_albero_binario_post_order(t_nodo *p)
{
  if (p!=nullptr)
  {
     stampa_albero_binario_post_order(p->sx);
     stampa_albero_binario_post_order(p->dx);
     cout << p->info << endl;
  }
}

// 5! = 5 * 4 * 3 * 2 * 1
// x alla y con x e y interi > 0

int main()
{
    //srand( time(0) ); //inizializziamo generatore numeri casuali

    t_nodo *radice=crea_albero_bilanciato(10);

    //stampa 'a mano'
    cout <<"                                ";
    cout << radice->info << endl; //41
    cout << "                    "
         << radice->sx->info //67
         << "                    "
         << radice->dx->info << endl; //78

    t_nodo *sx_l2 = radice->sx; //67
    t_nodo *dx_l2 = radice->dx; //78

    cout <<"               ";
    cout << sx_l2->sx->info << "        "  << sx_l2->dx->info
         << "         "
         << dx_l2->sx->info << "      "  << dx_l2->dx->info << endl;

    cout << "           ";
    cout << sx_l2->sx->sx->info << "         "   //0
         << sx_l2->dx->sx->info << "          "  //24
         << dx_l2->sx->sx->info << endl;         //62

    cout << "\n\n\n";
    stampa_albero_binario_pre_order(radice);
    return 0;
}
