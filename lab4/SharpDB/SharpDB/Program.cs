using Microsoft.Data.Sqlite;
using System;
using System.Data;
using System.IO;
using System.Linq;

namespace SharpDB
{
    class Program
    {
        private static string connectionString = "Data Source=inventory.db";
        private static string backupFolder = "backups";

        static void Main(string[] args)
        {
            InitializeDatabase();
            InitializeBackupFolder();

            Console.Clear();
            while (true)
            {
                Console.WriteLine("1. Режим без защиты");
                Console.WriteLine("2. Режим с защитой");
                Console.WriteLine("3. Создать копию БД");
                Console.WriteLine("4. Загрузить копию БД");
                Console.WriteLine("5. Выход");
                Console.Write("Выберите режим: ");

                var choice = Console.ReadLine();

                switch (choice)
                {
                    case "1":
                        UnsafeMode();
                        break;
                    case "2":
                        SafeMode();
                        break;
                    case "3":
                        CreateBackup();
                        break;
                    case "4":
                        RestoreBackup();
                        break;
                    case "5":
                        return;
                    default:
                        Console.WriteLine("Неверный выбор!");
                        break;
                }
            }
        }

        static void InitializeDatabase()
        {
            using (var connection = new SqliteConnection(connectionString))
            {
                connection.Open();

                var command = connection.CreateCommand();

               
                command.CommandText = @"
                    CREATE TABLE IF NOT EXISTS inventory (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        good_name TEXT NOT NULL,
                        amount INTEGER NOT NULL,
                        shop TEXT NOT NULL
                    )";
                command.ExecuteNonQuery();
                // Проверяем, есть ли уже данные в таблице
                command.CommandText = "SELECT COUNT(*) FROM inventory";
                var count = (long)command.ExecuteScalar();

                if (count == 0)
                {
                    var shops = new[] {"ЗооМир","Четыре Лапы","Питомец","ЗооГалерея","Друг"};
                    var goods = new[] {"Сухой корм","Ошейник","Когтеточка","Лежанка",
                "Переноска","Миска","Поилка","Витамины","Поводок"};

                    var random = new Random();

                    for (int i = 0; i < 15; i++)
                    {
                        command.CommandText = @"
                            INSERT INTO inventory (good_name, amount, shop)
                            VALUES (@good_name, @amount, @shop)";

                        command.Parameters.Clear();
                        command.Parameters.AddWithValue("@good_name", goods[random.Next(goods.Length)]);
                        command.Parameters.AddWithValue("@amount", random.Next(1, 100));
                        command.Parameters.AddWithValue("@shop", shops[random.Next(shops.Length)]);

                        command.ExecuteNonQuery();
                    }

                    Console.WriteLine("База данных товаров для животных инициализирована с 15 записями.");
                }
            }
        }

        static void InitializeBackupFolder()
        {
            if (!Directory.Exists(backupFolder))
            {
                Directory.CreateDirectory(backupFolder);
            }
        }

        static void CreateBackup()
        {
            try
            {
                if (!File.Exists("inventory.db"))
                {
                    Console.WriteLine("Ошибка: База данных не найдена!");
                    return;
                }

                // Создаем имя файла с временной меткой
                string timestamp = DateTime.Now.ToString("yyyyMMdd_HHmmss");
                string backupFileName = $"inventory_backup_{timestamp}.db";
                string backupPath = Path.Combine(backupFolder, backupFileName);

                // Копируем файл базы данных
                File.Copy("inventory.db", backupPath, true);

                Console.WriteLine($"\n=== Бэкап создан ===");
                Console.WriteLine($"Файл: {backupFileName}");
                Console.WriteLine($"Размер: {new FileInfo(backupPath).Length} байт");

                // Показываем список всех бэкапов
                ShowBackupList();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Ошибка при создании бэкапа: {ex.Message}");
            }
        }

        static void RestoreBackup()
        {
            try
            {
                var backupFiles = Directory.GetFiles(backupFolder, "inventory_backup_*.db").OrderByDescending(f => f).ToArray();

                if (backupFiles.Length == 0)
                {
                    Console.WriteLine("Нет доступных бэкапов для восстановления.");
                    return;
                }

                Console.WriteLine("\nДоступные бэкапы");
                for (int i = 0; i < backupFiles.Length; i++)
                {
                    var fileInfo = new FileInfo(backupFiles[i]);
                    Console.WriteLine($"{i + 1}. {Path.GetFileName(backupFiles[i])}");
                    Console.WriteLine($"   Размер: {fileInfo.Length} байт");
                    Console.WriteLine($"   Создан: {fileInfo.CreationTime}");
                }

                Console.Write($"\nВыберите бэкап для восстановления (1-{backupFiles.Length}): ");
                if (int.TryParse(Console.ReadLine(), out int choice) &&
                    choice >= 1 && choice <= backupFiles.Length)
                {
                    string selectedBackup = backupFiles[choice - 1];

                    System.GC.Collect();
                    System.GC.WaitForPendingFinalizers();

                    if (File.Exists("inventory.db"))
                    {
                        string currentBackup = Path.Combine(backupFolder,
                            $"inventory_before_restore_{DateTime.Now:yyyyMMdd_HHmmss}.db");
                        File.Copy("inventory.db", currentBackup, true);
                        Console.WriteLine($"\nТекущая БД сохранена как: {Path.GetFileName(currentBackup)}");
                    }

                    File.Copy(selectedBackup, "inventory.db", true);

                    Console.WriteLine("\nБэкап успешно восстановлен");
                    Console.WriteLine($"Восстановлен из: {Path.GetFileName(selectedBackup)}");
                }
                else
                {
                    Console.WriteLine("\nНеверный выбор!");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Ошибка при восстановлении бэкапа: {ex.Message}");
            }
        }

        static void ShowBackupList()
        {
            var backupFiles = Directory.GetFiles(backupFolder, "inventory_backup_*.db")
                                      .OrderByDescending(f => f)
                                      .ToArray();

            Console.WriteLine("\nСписок всех бэкапов");
            if (backupFiles.Length == 0)
            {
                Console.WriteLine("Бэкапов не найдено.");
                return;
            }

            foreach (var file in backupFiles.Take(5))
            {
                var fileInfo = new FileInfo(file);
                Console.WriteLine($"• {Path.GetFileName(file)}");
                Console.WriteLine($"  Создан: {fileInfo.CreationTime}, Размер: {fileInfo.Length} байт");
            }

            if (backupFiles.Length > 5)
            {
                Console.WriteLine($"... и еще {backupFiles.Length - 5} бэкапов");
            }
        }

        static void UnsafeMode()
        {
            using (var connection = new SqliteConnection(connectionString))
            {
                connection.Open();

                while (true)
                {
                    Console.WriteLine("\nДоступные команды:");
                    Console.WriteLine("1. Поиск товара по названию");
                    Console.WriteLine("2. Показать все товары");
                    Console.WriteLine("3. Назад в меню");
                    Console.Write("Выберите действие: ");

                    var choice = Console.ReadLine();

                    switch (choice)
                    {
                        case "1":
                            SearchGoodsUnsafe(connection);
                            break;
                        case "2":
                            ShowAllGoods(connection);
                            break;
                        case "3":
                            return;
                        default:
                            Console.WriteLine("Неверный выбор!");
                            break;
                    }
                }
            }
        }

        static void SearchGoodsUnsafe(SqliteConnection connection)
        {
            DemonstrateInjectionExamples();
            Console.Write("Введите название товара для поиска: ");
            var searchTerm = Console.ReadLine();

            var command = connection.CreateCommand();
            command.CommandText = $"SELECT * FROM inventory WHERE good_name = '{searchTerm}'";

            Console.WriteLine($"\nВыполняемый SQL: {command.CommandText}");
            Console.WriteLine("Результаты поиска:");

            try
            {
                using (var reader = command.ExecuteReader())
                {
                    DisplayResults(reader);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Ошибка: {ex.Message}");
            }
        }

        static void SafeMode()
        {
            using (var connection = new SqliteConnection(connectionString))
            {
                connection.Open();

                while (true)
                {
                    Console.WriteLine("\nДоступные команды:");
                    Console.WriteLine("1. Поиск товара по названию");
                    Console.WriteLine("2. Показать все товары");
                    Console.WriteLine("3. Назад в меню");
                    Console.Write("Выберите действие: ");

                    var choice = Console.ReadLine();

                    switch (choice)
                    {
                        case "1":
                            SearchGoodsSafe(connection);
                            break;
                        case "2":
                            ShowAllGoods(connection);
                            break;
                        case "3":
                            return;
                        default:
                            Console.WriteLine("Неверный выбор!");
                            break;
                    }
                }
            }
        }

        static void SearchGoodsSafe(SqliteConnection connection)
        {
            DemonstrateInjectionExamples();
            Console.Write("Введите название товара для поиска: ");
            var searchTerm = Console.ReadLine();

            var command = connection.CreateCommand();
            command.CommandText = "SELECT * FROM inventory WHERE good_name = @searchTerm";
            command.Parameters.AddWithValue("@searchTerm", searchTerm);

            Console.WriteLine($"\nВыполняемый SQL: {command.CommandText}");
            Console.WriteLine($"Параметр @searchTerm = '{searchTerm}'");
            Console.WriteLine("Результаты поиска:");

            try
            {
                using (var reader = command.ExecuteReader())
                {
                    DisplayResults(reader);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Ошибка: {ex.Message}");
            }
        }

        static void ShowAllGoods(SqliteConnection connection)
        {
            var command = connection.CreateCommand();
            command.CommandText = "SELECT * FROM inventory ORDER BY id";

            using (var reader = command.ExecuteReader())
            {
                DisplayResults(reader);
            }
        }

        static void DisplayResults(SqliteDataReader reader)
        {
            if (!reader.HasRows)
            {
                Console.WriteLine("Записи не найдены.");
                return;
            }

            Console.WriteLine("\nID\tТовар\t\tКоличество\tМагазин");
            Console.WriteLine("------------------------------------------------");

            while (reader.Read())
            {
                var goodName = reader.GetString(1);
                Console.WriteLine($"{reader.GetInt32(0)}\t{goodName.PadRight(10)}\t{reader.GetInt32(2)}\t\t{reader.GetString(3)}");
            }
        }

        static void DemonstrateInjectionExamples()
        {
            Console.WriteLine("\nПримеры SQL injection");
            Console.WriteLine("Обычный поиск: Ошейник");
            Console.WriteLine("Вывести все товары: ' OR 1=1; --");
            Console.WriteLine("Удалить все из магазина: '; DELETE FROM inventory WHERE shop = 'ЗооМир'; --");
            Console.WriteLine("Вывести структуру БД: ' UNION SELECT name, name, name, name FROM sqlite_master WHERE type='table'; --");
            Console.WriteLine("Изменить данные: '; UPDATE inventory SET amount = 0 WHERE shop = 'Друг'; --");
        }
    }
}