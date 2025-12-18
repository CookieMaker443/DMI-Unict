struct persona {
    char nome[20];
    int eta;
    char sesso;
};

int main() {
    struct persona p;
    strcpy(p.nome, "Mario Rossi");
    p.eta = 30;
    p.sesso = 'M';
    printf("Il nome è %s, l'età è %d e il sesso è %c\n", p.nome, p.eta, p.sesso);
    return 0;
}
