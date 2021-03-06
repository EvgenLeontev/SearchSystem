# SearchSystem

SearchSystem - поисковая система, которая осуществляет быстрый поиск по сохраненным в систему текстовым документам 
(аналог поисковой системы от Yandex или Google).

# Возможности

* Стоп-слова. 
Поисковая система исключает стоп-слова (слова, которые не учитываются системой и не влияют на выдачу - предлоги, 
междометия и т.д.) из документов и запроса.
  
* Минус-слова. 
Реализован учёт минус-слов (перед словом стоит знак "-"). Минус-слова исключают из результатов поиска документы, 
содержащие эти слова.

* TF-IDF.
Поисковая система ранжирует документы по TF-IDF. Вычисляется как часто встречается каждое слово запроса во всех наших 
документах (inverse document frequency или IDF) и доля каждого слова запроса во всех наших документах (term frequency 
или TF). Чем больше сумма всех произведений IDF каждого слова запроса и TF этого слова в документе, тем релевантнее 
документ. 

* Рейтинг документа.
Поисковая система вычисляет среднее значение оценок пользователей.

* Статус документа.
Документу присваивается статус (актуальный (ACTUAL), устаревший (IRRELEVANT), отклонённый (BANNED) или удалённый (REMOVED)).

* Многопоточность
Реализован последовательный и параллельный поиск документов.

# Использование

 Взаимодействие с поисковой системой возможно через функцию main в main.cpp.

1. Создайте объект класса SearchServer и передайте в конструктор строку стоп-слов.
2. Добавьте документы методом AddDocument с параметрами: идентификационный номер, документ, статус документа, оценки 
пользователей.
3. Выполните поиск методом FindTopDocuments с параметрами: параллельность поиска (необязательный, по умолчанию 
последовательный поиск), строка запроса (обязательный), статус документа (необязательный, по умолчанию ACTUAL).
4. Выведите результат в консоль функцией PrintDocument. 

Поисковая система возвращает MAX_RESULT_DOCUMENT_COUNT = 5 документов с самой высокой релевантностью.

# Требования

* C++17 
