import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'src/bloc/incident_bloc.dart';
import 'src/connection/websocket_service.dart';
import 'src/connection/bloc/websocket_bloc.dart';
import 'src/connection/bloc/websocket_event.dart';
import 'src/routes/app_routes.dart';
import 'src/utils/sims_colors.dart';
import 'src/repositories/settings_repository.dart';

void main() async {
  // Ensure Flutter bindings are initialized
  WidgetsFlutterBinding.ensureInitialized();

  // Initialize SettingsRepository before app starts
  await SettingsRepository.getInstance();

  // Suppress visual overflow indicators for minor rendering overflows
  FlutterError.onError = (FlutterErrorDetails details) {
    final exception = details.exception;
    final isOverflowError = exception is FlutterError &&
        !exception.diagnostics.any((node) => node.value.toString().contains('overflowed by'));

    if (isOverflowError) {
      FlutterError.presentError(details);
    } else {
      // Log the error but don't show the visual indicator
      FlutterError.dumpErrorToConsole(details);
    }
  };

  runApp(const SimsApp());
}

class SimsApp extends StatelessWidget {
  const SimsApp({super.key});

  @override
  Widget build(BuildContext context) {
    final webSocketService = WebSocketService();

    return MultiBlocProvider(
      providers: [
        BlocProvider(
          create: (context) => IncidentBloc(),
        ),
        BlocProvider(
          create: (context) => WebSocketBloc(webSocketService)
            ..add(const ConnectWebSocket()),
        ),
      ],
      child: MaterialApp.router(
        title: 'SIMS',
        debugShowCheckedModeBanner: false,
        theme: ThemeData(
          useMaterial3: true,
          colorScheme: ColorScheme.fromSeed(
            seedColor: SimsColors.background,
            brightness: Brightness.dark,
            surface: SimsColors.backgroundLight,
            background: SimsColors.background,
            primary: SimsColors.accentTactical,
            secondary: SimsColors.accentCyan,
          ),
          scaffoldBackgroundColor: SimsColors.background,
          fontFamily: 'SpaceGrotesk',
          textTheme: ThemeData.dark().textTheme.apply(
            bodyColor: SimsColors.white,
            displayColor: SimsColors.white,
            fontFamily: 'SpaceGrotesk',
          ).copyWith(
            headlineLarge: const TextStyle(
              fontSize: 32,
              fontWeight: FontWeight.w700,
              letterSpacing: 0.5,
              color: SimsColors.white,
              fontFamily: 'SpaceGrotesk',
            ),
            headlineMedium: const TextStyle(
              fontSize: 24,
              fontWeight: FontWeight.w700,
              letterSpacing: 0.5,
              color: SimsColors.white,
              fontFamily: 'SpaceGrotesk',
            ),
            bodyLarge: const TextStyle(
              fontSize: 16,
              fontWeight: FontWeight.w400,
              height: 1.6,
              color: SimsColors.white,
              fontFamily: 'SpaceGrotesk',
            ),
            bodyMedium: const TextStyle(
              fontSize: 14,
              fontWeight: FontWeight.w400,
              height: 1.6,
              color: SimsColors.white,
              fontFamily: 'SpaceGrotesk',
            ),
          ),
          appBarTheme: const AppBarTheme(
            backgroundColor: SimsColors.background,
            foregroundColor: SimsColors.white,
            elevation: 0,
            shadowColor: Colors.transparent,
            titleTextStyle: TextStyle(
              fontSize: 24,
              fontWeight: FontWeight.w700,
              letterSpacing: 0.5,
              color: SimsColors.white,
              fontFamily: 'SpaceGrotesk',
            ),
          ),
          floatingActionButtonTheme: const FloatingActionButtonThemeData(
            backgroundColor: SimsColors.accentTactical,
            foregroundColor: SimsColors.white,
            elevation: 4,
            shape: RoundedRectangleBorder(
              borderRadius: BorderRadius.all(Radius.circular(4)),
            ),
          ),
          cardTheme: CardThemeData(
            elevation: 2,
            shadowColor: Colors.black.withOpacity(0.3),
            color: SimsColors.backgroundLight,
            shape: const RoundedRectangleBorder(
              borderRadius: BorderRadius.all(Radius.circular(4)),
            ),
          ),
          inputDecorationTheme: InputDecorationTheme(
            border: const OutlineInputBorder(
              borderRadius: BorderRadius.all(Radius.circular(4)),
            ),
            filled: true,
            fillColor: SimsColors.backgroundLight,
            contentPadding: const EdgeInsets.symmetric(
              horizontal: 16,
              vertical: 12,
            ),
          ),
          elevatedButtonTheme: ElevatedButtonThemeData(
            style: ElevatedButton.styleFrom(
              backgroundColor: SimsColors.accentTactical,
              foregroundColor: SimsColors.white,
              elevation: 2,
              shadowColor: Colors.black.withOpacity(0.3),
              shape: const RoundedRectangleBorder(
                borderRadius: BorderRadius.all(Radius.circular(4)),
              ),
              textStyle: const TextStyle(
                fontSize: 16,
                fontWeight: FontWeight.w600,
                letterSpacing: 0.5,
                fontFamily: 'SpaceGrotesk',
              ),
              padding: const EdgeInsets.symmetric(
                horizontal: 24,
                vertical: 12,
              ),
            ),
          ),
        ),
        routerConfig: AppRoutes.createRouter(),
      ),
    );
  }
}
