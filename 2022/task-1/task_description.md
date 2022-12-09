Все 64 процесса, находящихся на разных ЭВМ сети, одновременно выдали запрос на вход в критическую секцию.
Реализовать программу, использующую широковещательный маркерный алгоритм для прохождения всеми процессами критических секций.
Критическая секция:

    ;
    if () {
        ;
        ;
    } else {
        ;
        Sleep ();
        ;
    }

Для межпроцессорных взаимодействий использовать средства MPI.
Получить временную оценку работы алгоритма. Оценить сколько времени потребуется для прохождения всеми критических секций, если маркером владеет нулевой процесс. Время старта равно 100, время передачи байта равно 1 (Ts=100,Tb=1). Процессорные операции, включая чтение из памяти и запись в память, считаются бесконечно быстрыми.