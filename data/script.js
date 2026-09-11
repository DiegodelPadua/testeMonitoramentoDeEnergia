console.log(
    "Dashboard Watt Vision iniciado"
);


// ==============================
// GRÁFICO
// ==============================

const contexto =
    document
        .getElementById("graficoPotencia")
        .getContext("2d");


const graficoPotencia =
    new Chart(

        contexto,

        {

            type: "line",

            data: {

                labels: [],

                datasets: [

                    {

                        label: "Potência (W)",

                        data: [],

                        borderWidth: 2,

                        tension: 0.4,

                        fill: true

                    }

                ]

            },


            options: {

                responsive: true,

                maintainAspectRatio: false,

                animation: false,

                plugins: {

                    legend: {
                        display: false
                    }

                },

                scales: {

                    x: {

                        ticks: {
                            color: "#657187"
                        },

                        grid: {
                            color: "#152033"
                        }

                    },


                    y: {

                        ticks: {
                            color: "#657187"
                        },

                        grid: {
                            color: "#152033"
                        }

                    }

                }

            }

        }

    );


// ==============================
// ATUALIZAÇÃO DOS DADOS
// ==============================

async function atualizarDados() {

    try {

        const resposta =
            await fetch(
                "/dados",
                {
                    cache: "no-store"
                }
            );


        if (!resposta.ok) {

            throw new Error(
                "Erro HTTP " +
                resposta.status
            );

        }


        const dados =
            await resposta.json();


        console.log(
            "Dados:",
            dados
        );


        // ==============================
        // CARDS
        // ==============================

        document
            .getElementById("tensao")
            .textContent =
            Number(dados.tensao)
                .toFixed(1);


        document
            .getElementById("corrente")
            .textContent =
            Number(dados.corrente)
                .toFixed(3);


        document
            .getElementById("potencia")
            .textContent =
            Number(dados.potencia)
                .toFixed(1);


        document
            .getElementById("energia")
            .textContent =
            Number(dados.energia)
                .toFixed(3);


        document
            .getElementById("frequencia")
            .textContent =
            Number(dados.frequencia)
                .toFixed(1);


        document
            .getElementById("fp")
            .textContent =
            Number(dados.fp)
                .toFixed(2);


        // ==============================
        // INDICADORES
        // ==============================

        document
            .getElementById(
                "energiaIndicador"
            )
            .textContent =
            Number(dados.energia)
                .toFixed(3)
            +
            " kWh";


        document
            .getElementById(
                "potenciaIndicador"
            )
            .textContent =
            Number(dados.potencia)
                .toFixed(1)
            +
            " W";


        document
            .getElementById(
                "tensaoIndicador"
            )
            .textContent =
            Number(dados.tensao)
                .toFixed(1)
            +
            " V";


        document
            .getElementById(
                "correnteIndicador"
            )
            .textContent =
            Number(dados.corrente)
                .toFixed(3)
            +
            " A";


        document
            .getElementById(
                "frequenciaIndicador"
            )
            .textContent =
            Number(dados.frequencia)
                .toFixed(1)
            +
            " Hz";


        document
            .getElementById(
                "fpIndicador"
            )
            .textContent =
            Number(dados.fp)
                .toFixed(2);


        // ==============================
        // GRÁFICO
        // ==============================

        const horario =
            new Date()
                .toLocaleTimeString();


        graficoPotencia
            .data
            .labels
            .push(horario);


        graficoPotencia
            .data
            .datasets[0]
            .data
            .push(
                dados.potencia
            );


        // Máximo de 30 pontos

        if (
            graficoPotencia
                .data
                .labels
                .length
            >
            30
        ) {

            graficoPotencia
                .data
                .labels
                .shift();


            graficoPotencia
                .data
                .datasets[0]
                .data
                .shift();

        }


        graficoPotencia.update();


        // ==============================
        // STATUS PZEM
        // ==============================

        document
            .getElementById(
                "statusPzem"
            )
            .textContent =
            "OK";

    }


    catch (erro) {

        console.error(
            "Erro:",
            erro
        );


        document
            .getElementById(
                "statusPzem"
            )
            .textContent =
            "Erro";

    }

}


// Primeira leitura

atualizarDados();


// Atualização a cada 2 segundos

setInterval(
    atualizarDados,
    2000
);