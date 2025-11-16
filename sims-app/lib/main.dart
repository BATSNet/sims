import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'src/bloc/incident_bloc.dart';
import 'src/connection/websocket_service.dart';
import 'src/connection/bloc/websocket_bloc.dart';
import 'src/connection/bloc/websocket_event.dart';
import 'src/routes/app_routes.dart';
import 'src/utils/sims_colors.dart';

void main() {
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
            seedColor: SimsColors.navyBlue,
            brightness: Brightness.dark,
            surface: SimsColors.navyBlue,
            background: SimsColors.navyBlue,
            primary: SimsColors.accentBlue,
            secondary: SimsColors.navyBlueLight,
          ),
          scaffoldBackgroundColor: SimsColors.navyBlue,
          fontFamily: 'SpaceGrotesk',
          textTheme: ThemeData.dark().textTheme.apply(
            bodyColor: SimsColors.white,
            displayColor: SimsColors.white,
            fontFamily: 'SpaceGrotesk',
          ),
          appBarTheme: const AppBarTheme(
            backgroundColor: SimsColors.navyBlue,
            foregroundColor: SimsColors.white,
            elevation: 0,
            titleTextStyle: TextStyle(
              fontSize: 24,
              fontWeight: FontWeight.bold,
              color: SimsColors.white,
              fontFamily: 'SpaceGrotesk',
            ),
          ),
          floatingActionButtonTheme: const FloatingActionButtonThemeData(
            backgroundColor: SimsColors.accentBlue,
            foregroundColor: SimsColors.white,
            shape: RoundedRectangleBorder(borderRadius: BorderRadius.zero),
          ),
          cardTheme: const CardTheme(
            elevation: 0,
            color: SimsColors.navyBlueLight,
            shape: RoundedRectangleBorder(borderRadius: BorderRadius.zero),
          ),
          inputDecorationTheme: const InputDecorationTheme(
            border: OutlineInputBorder(borderRadius: BorderRadius.zero),
            filled: true,
            fillColor: SimsColors.navyBlueLight,
          ),
          elevatedButtonTheme: ElevatedButtonThemeData(
            style: ElevatedButton.styleFrom(
              backgroundColor: SimsColors.accentBlue,
              foregroundColor: SimsColors.white,
              shape: const RoundedRectangleBorder(borderRadius: BorderRadius.zero),
              textStyle: const TextStyle(
                fontSize: 16,
                fontWeight: FontWeight.bold,
                fontFamily: 'SpaceGrotesk',
              ),
            ),
          ),
        ),
        routerConfig: AppRoutes.createRouter(),
      ),
    );
  }
}
