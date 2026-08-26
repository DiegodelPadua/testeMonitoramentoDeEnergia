// Função responsável por buscar
// os dados enviados pelo ESP32.

async function atualizarDados() {

    try {

        // Faz uma requisição para:
        //
        // http://IP_DO_ESP32/dados

        const resposta =
            await fetch("/dados");


        // Converte o JSON recebido
        // em um objeto JavaScript.

        const dados =
            await resposta.json();


        // Atualiza cada elemento
        // do dashboard.

        document.getElementById(
            "tensao"
        ).innerText =
            dados.tensao;


        document.getElementById(
            "corrente"
        ).innerText =
            dados.corrente;


        document.getElementById(
            "potencia"
        ).innerText =
            dados.potencia;


        document.getElementById(
            "energia"
        ).innerText =
            dados.energia;


        document.getElementById(
            "frequencia"
        ).innerText =
            dados.frequencia;


        document.getElementById(
            "fp"
        ).innerText =
            dados.fp;

    }

    catch (erro) {

        console.log(
            "Erro ao receber dados:",
            erro
        );

    }

}


// Atualiza os dados
// a cada 2 segundos.

setInterval(
    atualizarDados,
    2000
);


// Faz uma leitura imediatamente
// quando a página é aberta.

atualizarDados();